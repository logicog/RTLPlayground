import type { Port } from "../api";

/** A port has one owner. Selecting it in another group moves it in the draft. */
export function toggleAggregationPort(
  masks: readonly number[],
  selected: number,
  logicalPort: number,
): number[] {
  const bit = 1 << logicalPort;
  const removing = Boolean(masks[selected] & bit);
  return masks.map((mask, index) => {
    if (index === selected) return removing ? mask & ~bit : mask | bit;
    return removing ? mask : mask & ~bit;
  });
}

/** Release moved ports before assigning them, including swaps between groups. */
export function aggregationCommands(
  current: readonly number[],
  next: readonly number[],
  ports: readonly Port[],
): string[] {
  const command = (index: number, mask: number) => {
    const members = ports.filter((port) => mask & (1 << port.logPort)).map((port) => port.portNum);
    return [`lag ${index + 1}`, ...members].join(" ");
  };
  const retained = current.map((mask, index) => mask & next[index]);
  const releases = current.flatMap((mask, index) =>
    mask !== retained[index] ? [command(index, retained[index])] : [],
  );
  const assignments = next.flatMap((mask, index) =>
    mask !== retained[index] ? [command(index, mask)] : [],
  );
  return [...releases, ...assignments];
}
