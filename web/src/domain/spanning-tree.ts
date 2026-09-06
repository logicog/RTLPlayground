export interface SpanningTreePort {
  p: number;
  f: number;
  pc: string;
  prio: number;
  p2: number;
  st: number;
  role: number;
  db: string;
  dp: string;
  dc: string;
}

export interface SpanningTree {
  on: number;
  prio: number;
  rstp: number;
  hello: number;
  maxage: number;
  fwd: number;
  txhold: number;
  weRoot: number;
  rootPort: number;
  rootPrio: string;
  rootMac: string;
  cost: string;
  tc: string;
  ports: SpanningTreePort[];
}

export function portSettings(port: SpanningTreePort): Record<string, string> {
  let edge = "off";
  let guard = "none";
  if (port.f & 2) edge = "on";
  else if (port.f & 4) edge = "auto";
  if (port.f & 8) guard = "bpdu";
  else if (port.f & 16) guard = "root";

  return {
    enabled: port.f & 1 ? "on" : "off",
    cost: String(parseInt(port.pc, 16)),
    prio: String(port.prio),
    edge,
    guard,
    filter: port.f & 32 ? "on" : "off",
    p2p: ["auto", "on", "off"][port.p2],
  };
}

export function bridgeCommands(current: SpanningTree, form: FormData): string[] {
  const commands: string[] = [];
  const hello = Number(form.get("hello"));
  const maxAge = Number(form.get("maxage"));
  const forwardDelay = Number(form.get("fwd"));

  if (maxAge < 2 * (hello + 1) || maxAge > 2 * (forwardDelay - 1)) {
    throw new Error("Timers must satisfy 2 × (hello + 1) ≤ max age ≤ 2 × (forward delay − 1).");
  }

  for (const key of ["prio", "hello", "maxage", "fwd", "txhold"] as const) {
    if (Number(form.get(key)) !== current[key]) commands.push(`stp ${key} ${form.get(key)}`);
  }
  if (form.get("version") !== (current.rstp ? "rstp" : "stp")) {
    commands.push(`stp version ${form.get("version")}`);
  }
  if (form.has("on") !== Boolean(current.on)) {
    commands.push(`stp ${form.has("on") ? "on" : "off"}`);
  }
  return commands;
}

export function spanningPortCommands(current: SpanningTreePort, form: FormData): string[] {
  const commands: string[] = [];
  for (const [key, previous] of Object.entries(portSettings(current))) {
    if (form.get(key) === previous) continue;
    const setting = key === "enabled" ? "" : `${key} `;
    commands.push(`stp port ${current.p} ${setting}${form.get(key)}`);
  }
  return commands;
}
