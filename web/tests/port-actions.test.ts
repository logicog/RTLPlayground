import { describe, expect, it } from "vitest";
import type { Port } from "../src/api";
import { editMirrorPort, mirrorCommand, type MirrorSettings } from "../src/domain/port-actions";

const ports = [
  { portNum: 1, logPort: 7 },
  { portNum: 2, logPort: 3 },
  { portNum: 9, logPort: 0 },
] as Port[];
const disabled: MirrorSettings = { enabled: 0, mPort: 9, mirror_tx: "0", mirror_rx: "0" };

describe("mirroring port tools", () => {
  it("maps logical bits to physical command ports and preserves other sources", () => {
    const rx = editMirrorPort(disabled, ports[0], "rx");
    const tx = editMirrorPort(rx, ports[1], "tx");
    expect(rx.mirror_rx).toBe("10000000");
    expect(mirrorCommand(tx, ports)).toBe("mirror 9 1r 2t");
    expect(disabled.enabled).toBe(0);
  });

  it("toggles an existing source off and disables mirroring after the last source", () => {
    const both = editMirrorPort(disabled, ports[0], "both");
    expect(mirrorCommand(both, ports)).toBe("mirror 9 1");
    expect(mirrorCommand(editMirrorPort(both, ports[0], "both"), ports)).toBe("mirror off");
  });

  it("moving the destination removes that port from sources", () => {
    const first = editMirrorPort(disabled, ports[0], "both");
    const second = editMirrorPort(first, ports[1], "rx");
    const moved = editMirrorPort(second, ports[0], "destination");
    expect(mirrorCommand(moved, ports)).toBe("mirror 1 2r");
    expect(parseInt(moved.mirror_rx, 2) & (1 << 7)).toBe(0);
    expect(parseInt(moved.mirror_tx, 2) & (1 << 7)).toBe(0);
  });

  it("rejects a destination as source and leaves inspection unchanged", () => {
    expect(() => editMirrorPort(disabled, ports[2], "rx")).toThrow("destination");
    expect(editMirrorPort(disabled, ports[0], "inspect")).toBe(disabled);
  });
});
