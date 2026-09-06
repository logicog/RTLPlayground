/** Local preview transport. Imported only in development; never contacts hardware. */
const config =
  "ip 192.168.5.9\nnetmask 255.255.255.0\ngw 192.168.5.1\nvlan 1 1 2 3 4 5 6 7 8 9\nvlan 4 smart_local 1t 8t\nvlan 5 smart_wan 1t 8t\nvlan 1 mgmt\n";
let stored = config;
const ports = Array.from({ length: 9 }, (_, i) => ({
  portNum: i + 1,
  logPort: i,
  name: ["Uplink", "", "", "", "", "Workstation", "", "Home server", ""][i],
  isSFP: i === 8 ? 1 : 0,
  enabled: i === 8 ? 0 : 1,
  link: [6, 0, 0, 0, 0, 3, 0, 6, 0][i],
  adv: "111111",
  txG: "0x" + [846502, 0, 0, 0, 0, 190832, 0, 596318, 0][i].toString(16),
  rxG: "0x" + [982318, 0, 0, 0, 0, 340102, 0, 560123, 0][i].toString(16),
  txB: "0x0",
  rxB: "0x0",
}));
const vlans = [
  { id: 1, name: "" },
  { id: 4, name: "smart_local" },
  { id: 5, name: "smart_wan" },
];
const info = {
  ip_address: "192.168.5.9",
  ip_gateway: "192.168.5.1",
  ip_netmask: "255.255.255.0",
  hostname: "RTLPlayground-8ee809",
  hw_ver: "SWTGW218AS 8+1 Managed Switch",
  sw_ver: "v0.1.0-8a7b146",
  build_date: "2026-09-04 09:35:33",
  flash_size: "2 MB",
  mac_address: "06:05:16:8e:e8:09",
  syslog_server: "0.0.0.0:514",
};
const native = window.fetch.bind(window);
export const demoFetch: typeof fetch = async (input, init) => {
  let path: string;
  if (typeof input === "string") path = input;
  else if (input instanceof URL) path = input.href;
  else path = input.url;
  const url = new URL(path, location.origin);
  if (url.origin !== location.origin) throw new Error("External requests are disabled in preview.");
  if (
    !/^\/(information\.json|status\.json|vlanlist|vlan\.json|mtu\.json|bandwidth\.json|eee\.json|lag\.json|mirror\.json|stp\.json|l2\.json|counters\.json|config|cmd|login|reset|l2_del\.json|cmd_log)$/.test(
      url.pathname,
    )
  )
    return native(input, init);
  await new Promise((r) => setTimeout(r, 80));
  let body: unknown = "OK\n";
  switch (url.pathname) {
    case "/login": {
      const contentType = new Headers(init?.headers).get("Content-Type");
      if (contentType !== "application/x-www-form-urlencoded") {
        return new Response("Bad Request", { status: 400 });
      }
      const password = new URLSearchParams(String(init?.body)).get("pwd");
      const response = new Response("", { status: 200 });
      const destination = password === "preview" ? "/index.html" : "/login.html";
      Object.defineProperty(response, "url", { value: new URL(destination, location.origin).href });
      return response;
    }
    case "/information.json":
      body = info;
      break;
    case "/status.json":
      body = ports;
      break;
    case "/vlanlist":
      body = { mgmt: 1, vlan: vlans };
      break;
    case "/vlan.json": {
      const id = Number(url.searchParams.get("vid"));
      body = {
        members: id === 1 ? "0x207ffff" : "0x2000081",
        pvid: id === 1 ? "0x01ff" : "0x0000",
        name: vlans.find((v) => v.id === id)?.name || "",
      };
      break;
    }
    case "/mtu.json":
      body = ports.map((p) => ({ portNum: p.portNum, mtu: "0x3fff" }));
      break;
    case "/bandwidth.json":
      body = ports.map((p) => ({
        portNum: p.portNum,
        iLimited: 0,
        eLimited: 0,
        iBW: "000000",
        eBW: "000000",
        iFC: 1,
      }));
      break;
    case "/eee.json":
      body = ports.map((p) => ({
        portNum: p.portNum,
        isSFP: p.isSFP,
        eee: "111",
        eee_lp: p.link ? "111" : "000",
        active: 0,
      }));
      break;
    case "/lag.json":
      body = Array.from({ length: 4 }, () => ({
        members: "000000000",
        hash: "0x03",
      }));
      break;
    case "/mirror.json":
      body = {
        enabled: 0,
        mPort: 9,
        mirror_tx: "000000000",
        mirror_rx: "000000000",
      };
      break;
    case "/stp.json":
      body = {
        on: 0,
        prio: 8,
        rstp: 1,
        hello: 2,
        maxage: 20,
        fwd: 15,
        txhold: 6,
        weRoot: 1,
        rootPort: 0,
        rootPrio: "8000",
        rootMac: "0605168ee809",
        cost: "0x0",
        tc: "0x0",
        ports: ports.map((p) => ({
          p: p.portNum,
          f: 5,
          pc: "0x0",
          prio: 128,
          p2: 0,
          st: 3,
          role: 2,
          db: "80000605168ee809",
          dp: "8001",
          dc: "0x0",
        })),
      };
      break;
    case "/l2.json":
      body =
        Number(url.searchParams.get("idx")) === 0
          ? [
              {
                mac: "10:7b:44:23:18:01",
                vlan: "0x1",
                port: 0,
                idx: "0x10",
                type: "l",
              },
              {
                mac: "e0:63:da:8b:22:04",
                vlan: "0x4",
                port: 7,
                idx: "0x20",
                type: "s",
              },
            ]
          : [];
      break;
    case "/counters.json":
      body = Array.from({ length: 55 }, () => "0x0000000000000000");
      break;
    case "/l2_del.json":
      body = { result: 1 };
      break;
    case "/config":
      if (init?.method === "POST") {
        const file = (init.body as FormData).get("configuration") as Blob;
        stored = await file.text();
      } else body = stored;
      break;
    case "/cmd": {
      const cmd = String(init?.body || "");
      const name = cmd.match(/^port (\d+) name (.+)$/);
      if (name) ports[Number(name[1]) - 1].name = name[2];
      if (cmd.startsWith("hostname ")) info.hostname = cmd.slice(9);
      break;
    }
  }
  return new Response(typeof body === "string" ? body : JSON.stringify(body), {
    status: 200,
    headers: {
      "Content-Type": typeof body === "string" ? "text/plain" : "application/json",
    },
  });
};
