import { hex, type Port, type Vlan } from "../api";

export type PortMembership = "none" | "tagged" | "untagged";

export interface VlanPortDraft {
  membership: PortMembership;
  pvid: boolean;
}

export function nextMembership(state: VlanPortDraft): PortMembership {
  if (state.membership === "none") return "tagged";
  if (state.membership === "tagged") return "untagged";
  return state.pvid ? "tagged" : "none";
}

export interface VlanDraft {
  id: number;
  name: string;
  ports: Record<number, VlanPortDraft>;
}

export function setVlanMembership(
  draft: VlanDraft,
  portNumbers: number[],
  membership: PortMembership,
): VlanDraft {
  if (!["none", "tagged", "untagged"].includes(membership)) {
    throw new Error("Choose a valid membership mode.");
  }
  for (const port of portNumbers) {
    const state = draft.ports[port];
    if (!state) throw new Error(`Port ${port}: membership is unavailable.`);
    if (membership === "none" && state.pvid) {
      throw new Error(
        `Port ${port} uses this VLAN as PVID. Choose a replacement PVID before excluding it.`,
      );
    }
  }
  const ports = { ...draft.ports };
  for (const port of portNumbers) ports[port] = { ...ports[port], membership };
  return { ...draft, ports };
}

export function validateVlanNameBudget(vlans: Vlan[], draft: VlanDraft): void {
  // Firmware stores three hexadecimal ID digits, a name and a space per entry.
  const used = vlans
    .filter((vlan) => vlan.id !== draft.id && vlan.name)
    .reduce((total, vlan) => total + vlan.name.length + 4, 0);
  const next = used + (draft.name ? draft.name.length + 4 : 0);
  if (next >= 1024)
    throw new Error("VLAN name storage is full. Shorten the name before applying it.");
}

export function createVlanDraft(vlan: Vlan, ports: Port[]): VlanDraft {
  return {
    id: vlan.id,
    name: vlan.name,
    ports: Object.fromEntries(
      ports.map((port) => [
        port.portNum,
        {
          membership: membership(vlan, port.logPort),
          pvid: Boolean(hex(vlan.pvid) & (1 << port.logPort)),
        },
      ]),
    ),
  };
}

export function previewVlan(original: Vlan, draft: VlanDraft, ports: Port[]) {
  let members = hex(original.members);
  let pvid = hex(original.pvid);
  for (const port of ports) {
    const state = draft.ports[port.portNum];
    const bit = 1 << port.logPort;
    members &= ~(bit | (bit << 10));
    pvid &= ~bit;
    if (state.membership !== "none") members |= bit;
    if (state.membership === "untagged") members |= bit << 10;
    if (state.pvid) pvid |= bit;
  }
  return { id: draft.id, name: draft.name, members: members.toString(16), pvid: pvid.toString(16) };
}

export function vlanChanges(original: Vlan, draft: VlanDraft, ports: Port[]): string[] {
  const changes: string[] = [];
  if (!original.id) changes.push(`Create VLAN ${draft.id || "…"}`);
  if (draft.name !== original.name)
    changes.push(`Name: ${original.name || "unnamed"} → ${draft.name || "unnamed"}`);
  for (const port of ports) {
    const state = draft.ports[port.portNum];
    const before = membership(original, port.logPort);
    if (state.membership !== before)
      changes.push(`Port ${port.portNum}: ${before} → ${state.membership}`);
    if (state.pvid !== Boolean(hex(original.pvid) & (1 << port.logPort))) {
      changes.push(
        `Port ${port.portNum}: ${state.pvid ? `set PVID ${draft.id || "…"}` : "remove PVID"}`,
      );
    }
  }
  return changes;
}

export function membership(vlan: Vlan, logicalPort: number): PortMembership {
  const data = hex(vlan.members);
  const bit = 1 << logicalPort;
  if (!(data & bit)) return "none";
  return (data >> 10) & bit ? "untagged" : "tagged";
}

export function vlanCommands(original: Vlan, ports: Port[], draft: VlanDraft): string[] {
  const { id, name } = draft;
  const members: string[] = [];
  const defaults: string[] = [];

  if (!Number.isInteger(id) || id < 1 || id > 4094) {
    throw new Error("VLAN ID must be between 1 and 4094.");
  }
  if (original.id && original.id !== id) {
    throw new Error("Existing VLAN IDs cannot be changed. Create a new VLAN instead.");
  }
  if (name.toLowerCase() === "mgmt") {
    throw new Error("The name mgmt is reserved by the firmware for changing the management VLAN.");
  }
  if (original.name && !name) {
    throw new Error("The firmware cannot clear a VLAN name in place. Keep or replace the name.");
  }
  if (name && !/^[A-Za-z][A-Za-z0-9_]{0,23}$/.test(name)) {
    throw new Error(
      "Use a network name starting with a letter, followed by letters, digits or underscores.",
    );
  }

  for (const port of ports) {
    const state = draft.ports[port.portNum];
    if (!state || !["none", "tagged", "untagged"].includes(state.membership)) {
      throw new Error(`Port ${port.portNum}: membership is missing or invalid.`);
    }
    const mode = state.membership;
    const isDefault = state.pvid;
    const wasDefault = Boolean(hex(original.pvid) & (1 << port.logPort));

    if (mode === "tagged") members.push(`${port.portNum}t`);
    if (mode === "untagged") members.push(String(port.portNum));
    if (mode === "none" && isDefault) {
      throw new Error(`Port ${port.portNum}: its default VLAN must include that port.`);
    }
    if (wasDefault && !isDefault) {
      throw new Error(
        `Port ${port.portNum}: choose a replacement default VLAN in that VLAN's editor first.`,
      );
    }
    if (isDefault && !wasDefault) defaults.push(`pvid ${port.portNum} ${id}`);
  }

  if (!members.length) throw new Error("Select at least one member port.");
  const membershipChanged = ports.some(
    (port) => draft.ports[port.portNum].membership !== membership(original, port.logPort),
  );
  const nameChanged = name !== original.name;
  if (original.id && !membershipChanged && !nameChanged) return defaults;
  const command = [`vlan ${id}`, nameChanged ? name : "", ...members].filter(Boolean).join(" ");
  return [command, ...defaults];
}
