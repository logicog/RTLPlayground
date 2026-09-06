import type { Info } from "../api";

export function ipv4(value: string): boolean {
  const octets = value.split(".");
  return (
    octets.length === 4 && octets.every((octet) => /^\d{1,3}$/.test(octet) && Number(octet) <= 255)
  );
}

export function networkCommands(info: Info, managementVlan: number, form: FormData): string[] {
  const commands: string[] = [];
  const current = { netmask: info.ip_netmask, gw: info.ip_gateway, ip: info.ip_address };

  // Set the address last because the browser may lose its connection afterwards.
  for (const [key, previous] of Object.entries(current)) {
    const next = String(form.get(key));
    if (!ipv4(next)) throw new Error("Enter a valid IPv4 address in every network field.");
    if (next !== previous) commands.push(`${key} ${next}`);
  }
  const vlan = Number(form.get("vlan"));
  if (vlan !== managementVlan) commands.push(`vlan ${vlan} mgmt`);
  return commands;
}

export function redactPasswords(text: string): string {
  return text.replace(/^passwd .*$/gm, "passwd [hidden]");
}

export function validateConsoleCommand(command: string): void {
  const forbidden = /^(reset|reboot|flash|erase|default|reg|sfr|gpio|phy|spi)\b/i;
  if (forbidden.test(command)) {
    throw new Error("Hardware writes and resets are not available in this console.");
  }
  if (!command || /[\r\n\0]/.test(command)) {
    throw new Error("Enter one command on a single line.");
  }
}
