// Stable IDs keep their color across routes, sorting and page reloads.
// Higher IDs do not cycle a short palette.
const primaryHues = [218, 28, 274, 334, 188, 48, 8, 245];

export function entityColor(id: number): string {
  const hue = primaryHues[id - 1] ?? (218 + id * 137.508) % 360;
  return `--entity-hue: ${hue.toFixed(3)}`;
}
