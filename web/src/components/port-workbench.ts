import { html, nothing, type TemplateResult } from "lit";
import type { Context } from "../shared";
import type { Port } from "../api";
import { renderDeviceFront, renderLinkBadge, type PortAnnotation } from "./device-front";

interface PortWorkbench {
  ctx: Context;
  title: string;
  selected?: Port;
  showPortHeading?: boolean;
  onSelect: (event: Event) => void;
  annotations?: ReadonlyMap<number, PortAnnotation>;
  selectedPorts?: ReadonlySet<number>;
  toolbar?: TemplateResult;
  sidebar: TemplateResult;
  inspector: TemplateResult;
}

/** Every port feature keeps the device and selected connection in the same place. */
export function renderPortWorkbench(state: PortWorkbench) {
  const { selected } = state;
  return html`
    <section
      class="switch-workbench feature-workbench"
      aria-label="Switch ports"
    >
      ${
        state.toolbar
          ? html`
              <div class="workbench-networks">${state.toolbar}</div>
            `
          : nothing
      }
      ${renderDeviceFront({
        ports: state.ctx.ports,
        selectedPort: selected?.portNum ?? 0,
        overlay: "link",
        caption: state.title.toUpperCase(),
        annotations: state.annotations,
        selectedPorts: state.selectedPorts,
        onSelect: state.onSelect,
      })}
    </section>
    <div class="switch-details">
      <aside class="connection-browser feature-context">${state.sidebar}</aside>
      <section
        class="port-inspector"
        aria-label="Selected port controls"
      >
        ${
          selected && state.showPortHeading !== false
            ? html`
                <div class="inspector-heading">
                  <span class="port-index">${String(selected.portNum).padStart(2, "0")}</span>
                  <div>
                    <p>PORT ${selected.portNum} / ${state.title.toUpperCase()}</p>
                    <h2 id="port-details-title">${selected.name || `Port ${selected.portNum}`}</h2>
                  </div>
                  ${renderLinkBadge(selected)}
                </div>
              `
            : nothing
        }
        <div class="inspector-body">${state.inspector}</div>
      </section>
    </div>
  `;
}
