import type { Port } from "../api";

export interface PortDraft {
  name: string;
  enabled: boolean;
  speed: string;
  mtu: number;
}

export function portCommands(port: Port, currentMtu: number, draft: PortDraft): string[] {
  const commands: string[] = [];
  const prefix = `port ${port.portNum}`;

  if (draft.name !== port.name) {
    if (!draft.name || draft.name.length > 15 || !/^[A-Za-z0-9_.-]+$/.test(draft.name)) {
      throw new Error(
        "Use a non-empty port name containing letters, digits, dots, underscores or hyphens.",
      );
    }
    commands.push(`${prefix} name ${draft.name}`);
  }

  if (draft.enabled !== Boolean(port.enabled)) {
    commands.push(`${prefix} ${draft.enabled ? "on" : "off"}`);
  }
  if (draft.speed !== "keep" && draft.enabled) {
    commands.push(`${prefix} ${draft.speed}`);
  }
  if (draft.mtu !== currentMtu) {
    if (!Number.isInteger(draft.mtu) || draft.mtu < 64 || draft.mtu > 16383) {
      throw new Error(`Port ${port.portNum}: frame size must be between 64 and 16383 bytes.`);
    }
    commands.push(`mtu ${port.portNum} ${draft.mtu}`);
  }
  return commands;
}

export interface BandwidthState {
  portNum: number;
  iLimited: number;
  eLimited: number;
  iBW: string;
  eBW: string;
  iFC: number;
}

export interface BandwidthDraft {
  ingress: number;
  egress: number;
  flowControl: boolean;
}

export function bandwidthCommands(current: BandwidthState, draft: BandwidthDraft): string[] {
  const commands: string[] = [];
  const ingress = current.iLimited ? parseInt(current.iBW, 16) * 16 : 0;
  const egress = current.eLimited ? parseInt(current.eBW, 16) * 16 : 0;

  for (const [direction, previous, next] of [
    ["in", ingress, draft.ingress],
    ["out", egress, draft.egress],
  ] as const) {
    if (!Number.isInteger(next) || next < 0 || next > 10_000_000 || next % 16 !== 0) {
      throw new Error("Bandwidth must be between 0 and 10,000,000 Kbit/s, in steps of 16.");
    }
    if (next !== previous) {
      const value = next === 0 ? "off" : next.toString(16).padStart(8, "0");
      commands.push(`bw ${direction} ${current.portNum} ${value}`);
    }
  }

  const ingressChanged = draft.ingress !== ingress;
  const flowControlChanged = draft.flowControl !== Boolean(current.iFC);
  if (draft.ingress > 0 && (ingressChanged || flowControlChanged)) {
    commands.push(`bw in ${current.portNum} ${draft.flowControl ? "fc" : "drop"}`);
  }
  return commands;
}
