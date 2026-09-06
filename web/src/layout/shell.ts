import { html, nothing } from "lit";
import { chip } from "../shared";
import { icon, navigation } from "./navigation";
import type { Info } from "../api";

export interface ShellState {
  route: string;
  info?: Info;
  online: boolean;
  onThemeToggle: () => void;
  draft: boolean;
  unsaved: number;
  saving: boolean;
  saveError: string;
  verifySave: boolean;
  busy: boolean;
  onSave: () => Promise<void>;
}

function renderConfigurationStatus(state: ShellState) {
  let label = "Saved";
  if (state.unsaved) label = `Unsaved · ${state.unsaved}`;
  if (state.saveError) label = "Save failed";
  if (state.saving) label = "Saving…";
  return html`
    <div
      class="configuration-status"
      aria-label="Configuration status"
    >
      ${
        state.draft
          ? html`
              <span class="draft-status">Unapplied changes</span>
            `
          : nothing
      }
      <span
        class=${state.unsaved || state.saveError ? "startup-status pending" : "startup-status"}
        role="status"
        title=${state.saveError || "Startup configuration status for changes made in this tab"}
      >
        ${label}
      </span>
      ${
        state.unsaved
          ? html`
              <button
                type="button"
                class="header-save"
                ?disabled=${state.busy || state.saving}
                @click=${state.onSave}
              >
                ${state.verifySave ? "Verify save" : "Save"}
              </button>
            `
          : nothing
      }
    </div>
  `;
}

const sections = [
  { label: "Switch", routes: ["overview", "ports", "lag", "eee", "bandwidth"] },
  { label: "Networks", routes: ["vlans", "stp"] },
  { label: "Diagnostics", routes: ["statistics", "l2", "mirror"] },
  { label: "Settings", routes: ["system", "firmware"] },
];

export function renderNavigationHeader(state: ShellState) {
  return html`
    <header class="device-navigation">
      <a
        class="device-wordmark"
        href="#overview"
      >
        ${icon(navigation[1][2])}
        <span>
          RTL
          <strong>Playground</strong>
        </span>
      </a>
      <nav aria-label="Main navigation">
        ${sections.map(
          (section) => html`
            <a
              href=${`#${section.routes[0]}`}
              aria-current=${section.routes.includes(state.route) ? "page" : nothing}
            >
              ${section.label}
            </a>
          `,
        )}
      </nav>
      <div class="device-connection">
        ${renderConfigurationStatus(state)}
        ${chip(state.online ? "Connected" : "Offline", state.online)}
        <button
          class="icon-button"
          aria-label="Toggle color theme"
          @click=${state.onThemeToggle}
        >
          ${icon("M12 3a9 9 0 1 0 9 9 7 7 0 0 1-9-9")}
        </button>
      </div>
    </header>
  `;
}

export function renderTopbar(state: ShellState) {
  const section = sections.find((item) => item.routes.includes(state.route)) ?? sections[0];
  return html`
    <nav
      class="section-navigation"
      aria-label=${`${section.label} pages`}
    >
      ${section.routes.map(
        (route) => html`
          <a
            href=${`#${route}`}
            aria-current=${state.route === route ? "page" : nothing}
          >
            ${navigation.find(([id]) => id === route)?.[1]}
          </a>
        `,
      )}
    </nav>
  `;
}
