import { appendConfig } from "../api";

interface ConfigurationTransport {
  request(path: string, options?: RequestInit): Promise<string>;
}

export const autoSaveKey = "rtl-auto-save";

export function readAutoSave(): boolean {
  try {
    return localStorage.getItem(autoSaveKey) === "true";
  } catch {
    return false;
  }
}

const normalized = (text: string) => text.replace(/\r\n/g, "\n");

/** Preserve existing startup commands and verify every write by reading it back. */
export class StartupConfiguration {
  private baseline?: string;
  private unverified?: { body: string; commands: string[] };

  constructor(private transport: ConfigurationTransport) {}

  get needsVerification() {
    return Boolean(this.unverified);
  }

  async prepare(pendingCount: number) {
    if (this.unverified)
      throw new Error("Verify the previous configuration save before applying more changes.");
    if (pendingCount) return;
    const current = await this.transport.request("/config");
    appendConfig(current, []);
    this.baseline = current;
  }

  async save(commands: string[]): Promise<{ configuration: string; savedCount: number }> {
    const current = await this.transport.request("/config");
    if (this.unverified) return this.verify(current);
    if (this.baseline !== undefined && current !== this.baseline)
      throw new Error("Startup configuration changed outside this tab. Nothing was overwritten.");
    const body = appendConfig(current, commands);
    const form = new FormData();
    form.append(
      "configuration",
      new Blob([body], { type: "application/octet-stream" }),
      "config.txt",
    );
    this.unverified = { body, commands: [...commands] };
    await this.transport.request("/config", { method: "POST", body: form });
    return this.verify(await this.transport.request("/config"));
  }

  private verify(configuration: string) {
    const sent = this.unverified!;
    if (normalized(configuration) !== normalized(sent.body))
      throw new Error(
        "Save could not be verified. Changes remain marked unsaved; no automatic retry.",
      );
    this.baseline = configuration;
    this.unverified = undefined;
    return { configuration, savedCount: sent.commands.length };
  }
}
