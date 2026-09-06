# Switch web interface

Run `npm ci` and `npm run dev` in this directory. Vite serves the preview at
`http://127.0.0.1:5173`. Development uses visibly labelled simulated data and
does not proxy requests to the physical switch.

The main view combines the switch faceplate, a VLAN membership overlay and a
port inspector. Selecting a network highlights its member ports. PVID and
outgoing tagging are independent indicators. Failed VLAN reads show unknown
membership. Port settings remain drafts until the command review is confirmed.
Drafts survive polling, switching inspector tabs and choosing another port;
they are held in this page's memory, not saved across page reloads.

The Networks page reuses the faceplate for editing. Tagged, untagged and
excluded membership are explicit choices for the selected port. The faceplate
shows draft membership, with a before/after summary beside the editor. VLAN
drafts survive network selection and refresh. Before command review, the page
rereads the selected VLAN and rejects stale drafts. The editor protects active
PVIDs, reserves the firmware's `mgmt` keyword and checks VLAN name storage.
The bulk toolbar applies membership to all ports or a selected group. Enable
multiple selection and click ports on the faceplate, or use Select all / Clear
selection. Bulk edits change the draft only and preserve PVIDs; a bulk exclusion
is rejected in full if any target port uses this VLAN as PVID.

## Source and build

- `src/pages/overview.ts`: view state, VLAN reads and port inspector.
- `src/components/device-front.ts`: physical port rendering.
- `src/domain/port-networks.ts`: VLAN data validation and port membership.
- `src/pages/ports.ts`: port forms and per-port drafts.
- `src/pages/vlans.ts`: visual VLAN editor and review of pending changes.
- `src/domain/vlans.ts`: typed VLAN drafts and command validation.
- `src/layout/`: navigation and layout styles.
- `src/api.ts`: serialized requests to the firmware.

Keep source formatted and readable. `npm run build` type-checks, minifies and
packages production assets into the repository's `html/` directory. The
packager enforces per-file and total flash budgets. It does not flash firmware
or change the device. Firmware deployment follows the root `AGENTS.md` rules.

The root `make MACHINE=SWTGW218AS` command runs this frontend build before
generating `html_data.c` / `html_data.h` and packing the firmware. It installs
dependencies with `npm ci` when the lockfile or package manifest changes.
`make frontend` generates the UI alone. The ignored `html/` directory is build
output and is recreated automatically, including from a fresh checkout.

## Local verification

```sh
npm run check
npm test
npm run test:browser
npm run format:check
npm run build
```

Browser tests start an isolated development server on port 5175 and allow only
requests to that server. An installed Playwright Chromium browser is required;
set `PLAYWRIGHT_CHROMIUM_PATH` to reuse another compatible local Chromium
executable. The tests cover VLAN overlays, unknown data, keyboard selection,
mobile overflow and preservation of port drafts. They never apply changes to
the physical switch.
