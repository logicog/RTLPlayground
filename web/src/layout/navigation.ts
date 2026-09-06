import { html } from "lit";

export const navigation = [
  ["overview", "Port overview", "M3 3h7v7H3zm11 0h7v7h-7zM3 14h7v7H3zm11 0h7v7h-7z"],
  ["ports", "Ports", "M3 6h18v12H3zM7 10v4m5-4v4m5-4v4"],
  ["vlans", "VLANs", "M12 3v5M5 16v-5h14v5M2 16h6v5H2zm14 0h6v5h-6zM9 3h6v5H9z"],
  ["l2", "Learned addresses", "M4 4h16v16H4zM4 10h16M10 4v16"],
  ["stp", "Spanning tree", "M12 3v7m0 0-7 5m7-5 7 5M2 15h6v6H2zm14 0h6v6h-6zM9 3h6v5H9z"],
  ["lag", "Aggregation", "M4 5h6v6H4zm10 8h6v6h-6zM10 8h7v5M7 11v6h7"],
  ["mirror", "Mirroring", "M4 5h6v14H4zm10 0h6v14h-6zM10 12h4"],
  ["eee", "Energy saving", "M19 3C9 2 3 6 5 14s14 9 14-11ZM5 21l9-12"],
  ["bandwidth", "Bandwidth", "M4 5h16M4 12h16M4 19h16M8 2v6m8 1v6m-6 1v6"],
  ["statistics", "Statistics", "M4 20V10m8 10V4m8 16v-7"],
  [
    "system",
    "System settings",
    "M12 3v3m0 12v3M3 12h3m12 0h3M5 5l2 2m10 10 2 2M5 19l2-2M17 7l2-2M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8",
  ],
  ["firmware", "Firmware", "M12 16V3m-5 5 5-5 5 5M4 16v5h16v-5"],
];
export const icon = (path: string) => html`
  <svg
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    stroke-width="1.6"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
  >
    <path d=${path} />
  </svg>
`;
