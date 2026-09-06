import { html, nothing } from "lit";
import { speed, type Port } from "../api";
import { networkName, portNetwork, type VlanDetails } from "../domain/port-networks";
import { entityColor } from "../domain/entity-colors";
import { nextMembership } from "../domain/vlans";

export type PortOverlay = "link" | "vlan";
export interface PortAnnotation {
  label: string;
  tone?: "active" | "muted" | "warning";
  detail?: string;
  entityId?: number;
  actionLabel?: string;
  onActivate?: (event: Event) => void;
}

interface DeviceFrontState {
  ports: Port[];
  selectedPort: number;
  selectedPorts?: ReadonlySet<number>;
  overlay: PortOverlay;
  vlan?: VlanDetails;
  onSelect: (event: Event) => void;
  onCycleMembership?: (event: Event) => void;
  hint?: string;
  caption?: string;
  annotations?: ReadonlyMap<number, PortAnnotation>;
}

export function renderLinkBadge(port: Port) {
  let variant = "down";
  if (port.enabled && port.link) {
    variant = "gigabit";
    if (port.link === 1 || port.link === 2 || port.link === 4) variant = "megabit";
    if (port.link >= 5) variant = "multigig";
  }
  return html`
    <span class="link-badge link-${variant}">${speed(port)}</span>
  `;
}

function renderOverlayLabel(port: Port, state: DeviceFrontState) {
  const annotation = state.annotations?.get(port.portNum);
  if (annotation)
    return html`
      ${
        annotation.onActivate
          ? html`
              <button
                type="button"
                class="port-annotation annotation-${annotation.tone ?? "muted"} membership-toggle"
                data-port=${port.portNum}
                aria-label=${annotation.actionLabel ?? annotation.label}
                title=${annotation.actionLabel ?? annotation.label}
                @click=${annotation.onActivate}
              >
                ${annotation.label}
              </button>
            `
          : html`
              <span class="port-annotation annotation-${annotation.tone ?? "muted"}">
                ${annotation.label}
              </span>
            `
      }
      ${
        annotation.detail
          ? html`
              <small>${annotation.detail}</small>
            `
          : nothing
      }
    `;
  if (state.overlay === "link") return nothing;
  if (!state.vlan)
    return html`
      <span>Unknown</span>
    `;
  const network = portNetwork(state.vlan, port.logPort);
  const labels = { tagged: "T · Tagged", untagged: "U · Untagged", none: "Excluded" };
  const canToggle = Boolean(state.onCycleMembership);
  const next = nextMembership(network);
  const nextLabel = next === "none" ? "excluded" : next;
  return html`
    ${
      canToggle
        ? html`
            <button
              type="button"
              class="membership-badge membership-${network.membership} membership-toggle"
              data-port=${port.portNum}
              aria-label=${`Port ${port.portNum}: set ${nextLabel}`}
              title=${`Set ${nextLabel}`}
              @click=${state.onCycleMembership}
            >
              ${labels[network.membership]}
            </button>
          `
        : html`
            <span class="membership-badge membership-${network.membership}">
              ${labels[network.membership]}
            </span>
          `
    }
    ${
      network.pvid
        ? html`
            <small class="pvid-label">PVID</small>
          `
        : nothing
    }
  `;
}

function renderPort(port: Port, state: DeviceFrontState) {
  const connected = Boolean(port.enabled && port.link);
  const classes = ["front-port"];
  if (port.isSFP) classes.push("optical");
  if (connected) classes.push("link-up");
  if (!port.enabled) classes.push("port-disabled");
  if (state.overlay === "vlan" && state.vlan) {
    const { membership } = portNetwork(state.vlan, port.logPort);
    classes.push(membership === "none" ? "outside-network" : "in-network");
  }

  const entityId =
    state.annotations?.get(port.portNum)?.entityId ??
    (state.overlay === "vlan" ? state.vlan?.id : undefined);
  if (entityId !== undefined) classes.push("entity-color");

  const selected = state.selectedPorts?.has(port.portNum) ?? port.portNum === state.selectedPort;
  const label = `Port ${port.portNum}, ${port.name || "unnamed"}, ${speed(port)}${state.annotations?.has(port.portNum) ? `, ${state.annotations.get(port.portNum)!.label}` : ""}`;
  const content = html`
    <span class="front-port-number">
      ${String(port.portNum).padStart(2, "0")}
      ${
        state.selectedPorts
          ? html`
              <span
                class="front-selection"
                aria-hidden="true"
              >
                ${state.selectedPorts.has(port.portNum) ? "✓" : ""}
              </span>
            `
          : nothing
      }
      <span class="front-link-state">${connected ? "LINK" : "OFFLINE"}</span>
    </span>
    <span
      class="front-socket"
      aria-hidden="true"
    >
      <span class="front-pins"></span>
    </span>
    <strong>${port.name || (port.isSFP ? "SFP+" : `Port ${port.portNum}`)}</strong>
    ${renderLinkBadge(port)}
    <span class="front-port-reading">${renderOverlayLabel(port, state)}</span>
  `;
  const canToggle =
    (state.onCycleMembership && state.vlan) || state.annotations?.get(port.portNum)?.onActivate;
  if (canToggle) {
    return html`
      <div
        class=${`${classes.join(" ")} interactive-port`}
        style=${entityId === undefined ? nothing : entityColor(entityId)}
        data-port=${port.portNum}
        data-selected=${String(selected)}
      >
        <button
          type="button"
          class="front-port-select"
          data-port=${port.portNum}
          aria-pressed=${selected}
          aria-label=${label}
          @click=${state.onSelect}
        ></button>
        ${content}
      </div>
    `;
  }
  return html`
    <button
      type="button"
      class=${classes.join(" ")}
      style=${entityId === undefined ? nothing : entityColor(entityId)}
      data-port=${port.portNum}
      aria-pressed=${selected}
      aria-label=${label}
      @click=${state.onSelect}
    >
      ${content}
    </button>
  `;
}

export function renderDeviceFront(state: DeviceFrontState) {
  return html`
    <div class="device-front">
      <div class="faceplate-caption">
        <span>
          RTL
          <span class="muted">Playground</span>
        </span>
        <span>
          ${state.caption ?? (state.overlay === "vlan" ? `VLAN / ${state.vlan ? networkName(state.vlan) : "Membership unavailable"}` : "PORT STATUS")}
        </span>
      </div>
      <div
        class="front-ports"
        role="group"
        aria-label="Select a physical port"
      >
        ${state.ports.map((port) => renderPort(port, state))}
      </div>
      <div class="faceplate-footer">
        ${
          state.hint
            ? html`
                <span>${state.hint}</span>
              `
            : nothing
        }
        <span>
          <span class="link-key"></span>
          ${state.ports.filter((port) => port.enabled && port.link).length} links up
        </span>
      </div>
    </div>
  `;
}
