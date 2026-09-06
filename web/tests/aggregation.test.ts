import { describe, expect, it } from "vitest";
import type { Port } from "../src/api";
import { aggregationCommands, toggleAggregationPort } from "../src/domain/aggregation";

const ports = [
  { portNum: 1, logPort: 7 },
  { portNum: 2, logPort: 3 },
  { portNum: 6, logPort: 0 },
] as Port[];

describe("aggregation reassignment", () => {
  it("moves a port from another group while retaining every other member", () => {
    const current = [136, 1, 0, 0];
    const next = toggleAggregationPort(current, 1, 3);
    expect(next).toEqual([128, 9, 0, 0]);
    expect(current).toEqual([136, 1, 0, 0]);
    expect(aggregationCommands(current, next, ports)).toEqual(["lag 1 1", "lag 2 2 6"]);
  });

  it("releases both sides before swapping ports and leaves other groups alone", () => {
    const current = [8, 1, 128, 0];
    const moved = toggleAggregationPort(current, 1, 3);
    const swapped = toggleAggregationPort(moved, 0, 0);
    expect(aggregationCommands(current, swapped, ports)).toEqual([
      "lag 1",
      "lag 2",
      "lag 1 6",
      "lag 2 2",
    ]);
  });

  it("the latest draft selection wins and repeated clicks remove membership", () => {
    const first = toggleAggregationPort([0, 0, 0, 0], 0, 3);
    const second = toggleAggregationPort(first, 1, 3);
    expect(second).toEqual([0, 8, 0, 0]);
    expect(toggleAggregationPort(second, 1, 3)).toEqual([0, 0, 0, 0]);
    expect(aggregationCommands([0, 0, 0, 0], second, ports)).toEqual(["lag 2 2"]);
    expect(aggregationCommands(second, second, ports)).toEqual([]);
  });
});
