import { describe, expect, it } from "vitest";
import { StartupConfiguration } from "../src/domain/startup-configuration";

const original = "ip 192.168.5.9\nvlan 4 smart_local 1t 8t\npvid 8 4\n";

function device() {
  return {
    stored: original,
    writes: [] as string[],
    failAfterWrite: false,
    corruptReadback: false,
    async request(_path: string, options?: RequestInit) {
      if (options?.method === "POST") {
        const file = (options.body as FormData).get("configuration") as Blob;
        const body = await file.text();
        this.writes.push(body);
        this.stored = this.corruptReadback ? original : body;
        if (this.failAfterWrite) throw new Error("Connection lost");
        return "OK";
      }
      return this.stored;
    },
  };
}

describe("startup configuration writes", () => {
  it("preserves existing commands and verifies the appended changes", async () => {
    const switchDevice = device();
    const writer = new StartupConfiguration(switchDevice);
    await writer.prepare(0);
    const result = await writer.save(["port 2 name camera", "eee 3 off"]);
    expect(switchDevice.writes).toEqual([original + "port 2 name camera\neee 3 off\n"]);
    expect(result).toEqual({ configuration: switchDevice.stored, savedCount: 2 });
    expect(writer.needsVerification).toBe(false);
  });

  it("refuses to overwrite configuration changed by another client", async () => {
    const switchDevice = device();
    const writer = new StartupConfiguration(switchDevice);
    await writer.prepare(0);
    switchDevice.stored += "vlan 5 smart_wan 1t\n";
    await expect(writer.save(["eee 3 off"])).rejects.toThrow("outside this tab");
    expect(switchDevice.writes).toEqual([]);
  });

  it("verifies an uncertain completed write without sending it twice", async () => {
    const switchDevice = device();
    const writer = new StartupConfiguration(switchDevice);
    await writer.prepare(0);
    switchDevice.failAfterWrite = true;
    await expect(writer.save(["eee 3 off"])).rejects.toThrow("Connection lost");
    expect(writer.needsVerification).toBe(true);
    await expect(writer.prepare(1)).rejects.toThrow("Verify the previous");
    const result = await writer.save(["eee 3 off"]);
    expect(result.savedCount).toBe(1);
    expect(switchDevice.writes).toHaveLength(1);
    expect(writer.needsVerification).toBe(false);
  });

  it("keeps a failed readback unverified and never automatically resends", async () => {
    const switchDevice = device();
    switchDevice.corruptReadback = true;
    const writer = new StartupConfiguration(switchDevice);
    await writer.prepare(0);
    await expect(writer.save(["eee 3 off"])).rejects.toThrow("could not be verified");
    await expect(writer.save(["eee 3 off"])).rejects.toThrow("could not be verified");
    expect(switchDevice.writes).toHaveLength(1);
  });

  it("refuses an oversized configuration before touching flash", async () => {
    const switchDevice = device();
    switchDevice.stored = "a".repeat(4090) + "\n";
    const writer = new StartupConfiguration(switchDevice);
    await writer.prepare(0);
    await expect(writer.save(["eee 3 off"])).rejects.toThrow("4095-byte limit");
    expect(switchDevice.writes).toEqual([]);
  });
});
