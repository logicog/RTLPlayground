import { describe, expect, it } from "vitest";
import { decodeVlan, portNetwork } from "../src/domain/port-networks";

describe("VLAN port display", () => {
  it("uses logical port bits and ignores the firmware's VLAN-valid flag", () => {
    const vlan = decodeVlan(
      { id: 4, name: "smart_local" },
      {
        members: "0x2000081",
        pvid: "0x0000",
      },
    );
    expect(portNetwork(vlan, 0)).toEqual({ membership: "tagged", pvid: false });
    expect(portNetwork(vlan, 7)).toEqual({ membership: "tagged", pvid: false });
    expect(portNetwork(vlan, 1)).toEqual({ membership: "none", pvid: false });
    expect(portNetwork(vlan, 8)).toEqual({ membership: "none", pvid: false });
  });

  it("keeps PVID separate from outgoing tagging", () => {
    const vlan = decodeVlan(
      { id: 1, name: "" },
      {
        members: "0x2000803",
        pvid: "0x0001",
      },
    );
    expect(portNetwork(vlan, 0)).toEqual({ membership: "tagged", pvid: true });
    expect(portNetwork(vlan, 1)).toEqual({ membership: "untagged", pvid: false });
  });

  it("shows an inconsistent PVID even when the port is not a member", () => {
    const vlan = decodeVlan(
      { id: 5, name: "smart_wan" },
      {
        members: "0x2000000",
        pvid: "0x0004",
      },
    );
    expect(portNetwork(vlan, 2)).toEqual({ membership: "none", pvid: true });
  });

  it("rejects missing or malformed data instead of showing ports as excluded", () => {
    for (const response of [{}, { members: "garbage", pvid: "0x0" }, { members: "0x0" }]) {
      expect(() => decodeVlan({ id: 4, name: "smart_local" }, response)).toThrow("unavailable");
    }
  });
});
