import type { Port } from "../api";

export interface MirrorSettings {
  enabled: number;
  mPort: number;
  mirror_tx: string;
  mirror_rx: string;
}

export type MirrorAction = "both" | "rx" | "tx" | "destination" | "remove" | "inspect";

/** Clicking a source applies the chosen direction; clicking it again removes it. */
export function editMirrorPort(
  current: MirrorSettings,
  port: Port,
  action: MirrorAction,
): MirrorSettings {
  if (action === "inspect") return current;
  const bit = 1 << port.logPort;
  let tx = parseInt(current.mirror_tx, 2);
  let rx = parseInt(current.mirror_rx, 2);
  let destination = current.mPort;
  if (action === "destination") destination = port.portNum;
  else if (destination === port.portNum)
    throw new Error("The destination cannot also be a source. Choose another destination first.");
  const wantsTx = action === "both" || action === "tx";
  const wantsRx = action === "both" || action === "rx";
  const alreadySelected = Boolean(tx & bit) === wantsTx && Boolean(rx & bit) === wantsRx;
  tx &= ~bit;
  rx &= ~bit;
  if (!alreadySelected && action !== "destination") {
    if (wantsTx) tx |= bit;
    if (wantsRx) rx |= bit;
  }
  return {
    enabled: Number(Boolean(tx || rx)),
    mPort: destination,
    mirror_tx: tx.toString(2),
    mirror_rx: rx.toString(2),
  };
}

export function mirrorCommand(state: MirrorSettings, ports: Port[]): string {
  if (!state.enabled) return "mirror off";
  const sources = ports.flatMap((port) => {
    if (port.portNum === state.mPort) return [];
    const tx = Boolean(parseInt(state.mirror_tx, 2) & (1 << port.logPort));
    const rx = Boolean(parseInt(state.mirror_rx, 2) & (1 << port.logPort));
    if (tx && rx) return [String(port.portNum)];
    if (tx) return [`${port.portNum}t`];
    if (rx) return [`${port.portNum}r`];
    return [];
  });
  if (!sources.length) throw new Error("Choose a source port on the switch.");
  return `mirror ${state.mPort} ${sources.join(" ")}`;
}
