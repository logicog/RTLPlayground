import { loginRequest, verifyLoginResponse } from "./domain/session";

/** One HTTP operation at a time: the firmware shares request buffers globally. */
export class ApiError extends Error {
  constructor(
    message: string,
    public status = 0,
  ) {
    super(message);
  }
}
export class Api {
  private tail_: Promise<unknown> = Promise.resolve();
  private transport: typeof fetch = (...args) => fetch(...args);
  pending = 0;

  useTransport(transport: typeof fetch): void {
    this.transport = transport;
  }
  signIn(password: string): Promise<void> {
    return this.exclusive(async () => {
      const response = await this.transport("/login", loginRequest(password));
      await verifyLoginResponse(response);
    });
  }
  exclusive<T>(work: () => Promise<T>): Promise<T> {
    this.pending++;
    const result = this.tail_.then(work);
    this.tail_ = result
      .catch(() => undefined)
      .finally(() => {
        this.pending--;
      });
    return result;
  }
  request(path: string, options: RequestInit = {}, timeout = 10000): Promise<string> {
    return this.exclusive(async () => {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), timeout);
      try {
        const response = await this.transport(path, {
          ...options,
          credentials: "same-origin",
          cache: "no-store",
          signal: controller.signal,
        });
        if (response.status === 401 || response.url.includes("/login.html")) {
          window.dispatchEvent(new Event("session-expired"));
          throw new ApiError("Session expired. Sign in again; your draft stays in this tab.", 401);
        }
        const text = await response.text();
        if (!response.ok)
          throw new ApiError(text.trim() || `Request failed (${response.status})`, response.status);
        return text;
      } catch (error) {
        if (error instanceof ApiError) throw error;
        throw new ApiError(
          options.method === "POST"
            ? "Connection interrupted. The change may have reached the switch. Check its state before trying again."
            : "Switch is not responding. Check your connection and try again.",
        );
      } finally {
        clearTimeout(timer);
      }
    });
  }
  async json<T>(path: string): Promise<T> {
    const text = await this.request(path);
    try {
      return JSON.parse(text) as T;
    } catch {
      throw new ApiError(
        "The switch returned an incomplete or invalid response. Refresh to try again.",
      );
    }
  }
  async command(command: string): Promise<string> {
    if (new TextEncoder().encode(command).length > 127 || /[\r\n\0]/.test(command))
      throw new ApiError("Commands must be a single line, at most 127 bytes.");
    const result = await this.request("/cmd", {
      method: "POST",
      body: command,
    });
    if (/^\s*(error|invalid|unknown|syntax)/im.test(result)) throw new ApiError(result.trim());
    return result;
  }
}
export const api = new Api();
export const hex = (value: string | number = 0) =>
  typeof value === "number" ? value : parseInt(value, 16) || 0;
export const count = (value = "0") => {
  try {
    return BigInt(value).toLocaleString();
  } catch {
    return "—";
  }
};
export const speed = (port: Port) =>
  !port.enabled
    ? "Disabled"
    : ["No link", "10 Mbps", "100 Mbps", "1 Gbps", "500 Mbps", "10 Gbps", "2.5 Gbps", "5 Gbps"][
        port.link
      ] || "Unknown";
export interface Port {
  portNum: number;
  logPort: number;
  name: string;
  isSFP: number;
  enabled: number;
  link: number;
  adv?: string;
  txG: string;
  rxG: string;
  txB: string;
  rxB: string;
  [key: string]: unknown;
}
export interface Info {
  ip_address: string;
  ip_gateway: string;
  ip_netmask: string;
  hostname: string;
  hw_ver: string;
  sw_ver: string;
  flash_size: string;
  mac_address: string;
  build_date: string;
  syslog_server: string;
}
export interface Vlan {
  id: number;
  name: string;
  members?: string;
  pvid?: string;
}
export interface VlanList {
  mgmt: number;
  vlan: Vlan[];
}
export const download = (name: string, contents: string) => {
  const url = URL.createObjectURL(new Blob([contents], { type: "text/plain" }));
  const link = document.createElement("a");
  link.href = url;
  link.download = name;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
};
export function appendConfig(saved: string, commands: string[]): string {
  if (!saved.trim() || /[\x00-\x08\x0b\x0c\x0e-\x1f\xff]/.test(saved))
    throw new Error("Saved configuration is empty or malformed. Nothing was written.");
  const result =
    saved +
    (saved.endsWith("\n") ? "" : "\n") +
    commands.join("\n") +
    (commands.length ? "\n" : "");
  if (new TextEncoder().encode(result).length >= 4096)
    throw new Error(
      "Configuration exceeds the 4095-byte limit. Export and review it; nothing was written.",
    );
  return result;
}
export function validateImage(data: Uint8Array): void {
  if (data.length !== 524288)
    throw new Error("Choose a 512 KiB RTLPlayground .bin image. OEM installers are not supported.");
  if (data[0] !== 0 || data[1] !== 0x40) throw new Error("Invalid RTLPlayground image header.");
  let crc = 0;
  for (const byte of data) {
    crc ^= byte;
    for (let i = 0; i < 8; i++) crc = (crc >> 1) ^ (crc & 1 ? 0xa001 : 0);
  }
  if (crc !== 0xb001) throw new Error("Image checksum failed. Nothing was uploaded.");
}
