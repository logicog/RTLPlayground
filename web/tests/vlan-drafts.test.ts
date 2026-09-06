import { describe, expect, it } from "vitest";
import type { Port } from "../src/api";
import {
  createVlanDraft,
  previewVlan,
  vlanCommands,
  validateVlanNameBudget,
  setVlanMembership,
} from "../src/domain/vlans";
import { portNetwork } from "../src/domain/port-networks";

const ports: Port[] = [
  {
    portNum: 1,
    logPort: 7,
    name: "Uplink",
    enabled: 1,
    link: 6,
    isSFP: 0,
    txG: "0",
    rxG: "0",
    txB: "0",
    rxB: "0",
  },
  {
    portNum: 8,
    logPort: 0,
    name: "Server",
    enabled: 1,
    link: 6,
    isSFP: 0,
    txG: "0",
    rxG: "0",
    txB: "0",
    rxB: "0",
  },
];
const original = { id: 4, name: "smart_local", members: "0x2000281", pvid: "0x0" };

describe("VLAN drafts", () => {
  it("bulk edits change only the selected memberships, preserving PVID and the original draft", () => {
    const draft = createVlanDraft({ ...original, pvid: "0x1" }, ports);
    const changed = setVlanMembership(draft, [8], "untagged");
    expect(changed.ports[8]).toEqual({ membership: "untagged", pvid: true });
    expect(changed.ports[1]).toEqual(draft.ports[1]);
    expect(draft.ports[8]).toEqual({ membership: "tagged", pvid: true });
  });

  it("rejects an entire bulk exclusion when any selected port uses this PVID", () => {
    const draft = createVlanDraft({ ...original, pvid: "0x1" }, ports);
    expect(() => setVlanMembership(draft, [1, 8], "none")).toThrow("replacement PVID");
    expect(draft.ports[1].membership).toBe("tagged");
    expect(draft.ports[8]).toEqual({ membership: "tagged", pvid: true });
  });

  it("does not issue commands for an unchanged network", () => {
    expect(vlanCommands(original, ports, createVlanDraft(original, ports))).toEqual([]);
  });

  it("preserves other members and uses physical port numbers in commands", () => {
    const draft = createVlanDraft(original, ports);
    draft.ports[8].membership = "untagged";
    expect(vlanCommands(original, ports, draft)).toEqual(["vlan 4 1t 8"]);
    const preview = previewVlan(original, draft, ports);
    expect(portNetwork(preview, 0).membership).toBe("untagged");
    expect(portNetwork(preview, 7).membership).toBe("tagged");
    expect(portNetwork(preview, 9).membership).toBe("tagged");
    expect(draft.ports[8].pvid).toBe(false);
  });

  it("changing only PVID does not rewrite VLAN membership", () => {
    const draft = createVlanDraft(original, ports);
    draft.ports[8].pvid = true;
    expect(vlanCommands(original, ports, draft)).toEqual(["pvid 8 4"]);
  });

  it("refuses to remove an existing PVID without a replacement", () => {
    const current = { ...original, pvid: "0x80" };
    const draft = createVlanDraft(current, ports);
    draft.ports[1].pvid = false;
    expect(() => vlanCommands(current, ports, draft)).toThrow("replacement default VLAN");
  });

  it("rejects reserved names, renamed IDs and missing port data", () => {
    const draft = createVlanDraft(original, ports);
    expect(() => vlanCommands(original, ports, { ...draft, name: "mgmt" })).toThrow("reserved");
    expect(() => vlanCommands(original, ports, { ...draft, id: 5 })).toThrow("cannot be changed");
    expect(() => vlanCommands(original, ports, { ...draft, ports: {} })).toThrow(
      "missing or invalid",
    );
    expect(() => vlanCommands(original, ports, { ...draft, name: "" })).toThrow("cannot clear");
  });

  it("keeps name entries and their terminator within the firmware's buffer", () => {
    const names = Array.from({ length: 36 }, (_, index) => ({
      id: index + 10,
      name: "a".repeat(24),
    }));
    const draft = { ...createVlanDraft(original, ports), name: "b".repeat(24) };
    expect(() => validateVlanNameBudget(names, draft)).toThrow("storage is full");
    expect(() => validateVlanNameBudget(names.slice(1), draft)).not.toThrow();
  });
});
