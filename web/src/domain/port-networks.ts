import { hex, type Vlan } from "../api";
import { membership } from "./vlans";

export interface VlanDetails extends Vlan {
  members: string;
  pvid: string;
}

export function networkName(vlan: Vlan): string {
  return vlan.name || `VLAN ${vlan.id}`;
}

export function portNetwork(vlan: VlanDetails, logicalPort: number) {
  return {
    membership: membership(vlan, logicalPort),
    pvid: Boolean(hex(vlan.pvid) & (1 << logicalPort)),
  };
}

export function decodeVlan(vlan: Vlan, response: Partial<VlanDetails>): VlanDetails {
  for (const field of [response.members, response.pvid]) {
    if (typeof field !== "string" || !/^(0x)?[0-9a-f]+$/i.test(field)) {
      throw new Error(`VLAN ${vlan.id}: membership data is unavailable.`);
    }
  }
  return { ...vlan, members: response.members!, pvid: response.pvid! };
}
