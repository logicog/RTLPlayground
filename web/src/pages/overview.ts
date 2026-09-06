import { LitElement, html, nothing, type PropertyValues } from "lit";
import { api, count, speed, type Port, type VlanList } from "../api";
import type { Context } from "../shared";
import { entityColor } from "../domain/entity-colors";
import { decodeVlan, networkName, portNetwork, type VlanDetails } from "../domain/port-networks";
import { renderDeviceFront, renderLinkBadge, type PortOverlay } from "../components/device-front";
import "./ports";
import "./overview.css";

export class OverviewPage extends LitElement {
  static properties = {
    ctx: { attribute: false },
    vlans: { attribute: false },
    selectedVlan: { state: true },
    overlay: { state: true },
    detailsTab: { state: true },
    settingsOpened: { state: true },
    networks: { state: true },
    networksLoading: { state: true },
    networksError: { state: true },
  };

  ctx!: Context;
  vlans!: VlanList;
  private selectedVlan = 0;
  private overlay: PortOverlay = "link";
  private detailsTab = "status";
  private settingsOpened = false;
  private networks: VlanDetails[] = [];
  private networksLoading = true;
  private networksError = "";
  private networkGeneration = 0;

  protected createRenderRoot() {
    return this;
  }

  protected updated(changed: PropertyValues) {
    if (changed.has("vlans")) void this.loadNetworks();
  }

  disconnectedCallback() {
    super.disconnectedCallback();
    this.networkGeneration++;
  }

  private async loadNetworks() {
    const generation = ++this.networkGeneration;
    this.networksLoading = true;
    this.networksError = "";
    const networks: VlanDetails[] = [];

    try {
      for (const vlan of this.vlans.vlan) {
        if (!this.isConnected || generation !== this.networkGeneration) return;
        const response = await api.json<Partial<VlanDetails>>(`/vlan.json?vid=${vlan.id}`);
        networks.push(decodeVlan(vlan, response));
      }
      if (generation !== this.networkGeneration) return;
      this.networks = networks;
      if (!networks.some((vlan) => vlan.id === this.selectedVlan)) {
        this.selectedVlan = networks[0]?.id ?? 0;
      }
    } catch (error) {
      if (generation !== this.networkGeneration) return;
      this.networksError = (error as Error).message;
    } finally {
      if (generation === this.networkGeneration) this.networksLoading = false;
    }
  }

  private selectPort = (event: Event) => {
    this.ctx.selectPort(Number((event.currentTarget as HTMLButtonElement).dataset.port));
  };

  private selectOverlay(event: Event) {
    this.overlay = (event.currentTarget as HTMLButtonElement).dataset.overlay as PortOverlay;
  }

  private selectNetwork(event: Event) {
    this.selectedVlan = Number((event.currentTarget as HTMLButtonElement).dataset.vlan);
    this.overlay = "vlan";
  }

  private selectDetailsTab(event: Event) {
    this.detailsTab = (event.currentTarget as HTMLButtonElement).dataset.tab!;
    if (this.detailsTab === "settings") this.settingsOpened = true;
  }

  private renderOverlayPicker() {
    return html`
      <div
        class="segmented"
        role="group"
        aria-label="Port overlay"
      >
        ${(
          [
            ["link", "Link speed"],
            ["vlan", "VLAN membership"],
          ] as const
        ).map(
          ([id, label]) => html`
            <button
              aria-pressed=${this.overlay === id}
              data-overlay=${id}
              @click=${this.selectOverlay}
            >
              ${label}
            </button>
          `,
        )}
      </div>
    `;
  }

  private renderNetworkPicker() {
    if (this.networksLoading)
      return html`
        <p role="status">Reading VLAN membership…</p>
      `;
    if (this.networksError)
      return html`
        <div
          class="notice error"
          role="alert"
        >
          <span>${this.networksError}</span>
          <button @click=${this.loadNetworks}>Retry VLAN read</button>
        </div>
      `;

    return html`
      <div
        class="network-selector"
        role="group"
        aria-label="Highlight a VLAN"
      >
        ${this.networks.map(
          (vlan) => html`
            <button
              class="network-choice entity-color"
              style=${entityColor(vlan.id)}
              aria-pressed=${this.overlay === "vlan" && this.selectedVlan === vlan.id}
              data-vlan=${vlan.id}
              @click=${this.selectNetwork}
            >
              <span
                class="network-swatch"
                aria-hidden="true"
              ></span>
              <strong>${networkName(vlan)}</strong>
              <span>${vlan.id}${vlan.id === this.vlans.mgmt ? " · Management" : ""}</span>
            </button>
          `,
        )}
      </div>
    `;
  }

  private renderPortNetworks(port: Port) {
    if (this.networksLoading)
      return html`
        <p>Reading VLAN membership…</p>
      `;
    if (this.networksError)
      return html`
        <p>VLAN membership unavailable. Retry the VLAN read.</p>
      `;
    const networks = this.networks.filter((vlan) => {
      const state = portNetwork(vlan, port.logPort);
      return state.membership !== "none" || state.pvid;
    });

    return html`
      <div class="port-network-list">
        ${
          networks.length
            ? networks.map((vlan) => {
                const state = portNetwork(vlan, port.logPort);
                return html`
                  <div
                    class="port-network entity-color"
                    style=${entityColor(vlan.id)}
                  >
                    <span
                      class="network-swatch"
                      aria-hidden="true"
                    ></span>
                    <div>
                      <strong>${networkName(vlan)}</strong>
                      <small>VLAN ${vlan.id}</small>
                    </div>
                    <span class="membership-badge membership-${state.membership}">
                      ${state.membership === "none" ? "Not a member" : state.membership}
                    </span>
                    ${
                      state.pvid
                        ? html`
                            <span class="pvid-label">PVID</span>
                          `
                        : nothing
                    }
                  </div>
                `;
              })
            : html`
                <p>No VLAN membership reported.</p>
              `
        }
      </div>
      <p class="field-help">
        PVID classifies incoming untagged traffic. Tagged / untagged describes outgoing traffic.
      </p>
      <a
        class="text-link"
        href="#vlans"
      >
        Edit VLAN membership ↗
      </a>
    `;
  }

  private renderPortStatus(port: Port) {
    return html`
      <dl class="port-facts">
        <div>
          <dt>Link</dt>
          <dd>${speed(port)}</dd>
        </div>
        <div>
          <dt>Interface</dt>
          <dd>${port.isSFP ? "SFP+" : "Ethernet"}</dd>
        </div>
        <div>
          <dt>RX packets</dt>
          <dd>${count(port.rxG)}</dd>
        </div>
        <div>
          <dt>TX packets</dt>
          <dd>${count(port.txG)}</dd>
        </div>
        <div>
          <dt>RX errors</dt>
          <dd>${count(port.rxB)}</dd>
        </div>
        <div>
          <dt>TX errors</dt>
          <dd>${count(port.txB)}</dd>
        </div>
      </dl>
      <h3>Networks on this port</h3>
      ${this.renderPortNetworks(port)}
    `;
  }

  private renderPortDetails(port: Port) {
    return html`
      <section
        class="port-inspector"
        aria-labelledby="port-details-title"
      >
        <div class="inspector-heading">
          <span class="port-index">${String(port.portNum).padStart(2, "0")}</span>
          <div>
            <p>PORT ${port.portNum}${port.isSFP ? " / SFP+" : " / ETHERNET"}</p>
            <h2 id="port-details-title">${port.name || `Port ${port.portNum}`}</h2>
          </div>
          ${renderLinkBadge(port)}
        </div>
        <div
          class="inspector-tabs"
          role="group"
          aria-label="Port details view"
        >
          <button
            aria-pressed=${this.detailsTab === "status"}
            data-tab="status"
            @click=${this.selectDetailsTab}
          >
            Status & networks
          </button>
          <button
            aria-pressed=${this.detailsTab === "settings"}
            data-tab="settings"
            @click=${this.selectDetailsTab}
          >
            Configure port
          </button>
        </div>
        <div class="inspector-body">
          <div ?hidden=${this.detailsTab !== "status"}>${this.renderPortStatus(port)}</div>
          <div ?hidden=${this.detailsTab !== "settings"}>
            ${
              this.settingsOpened
                ? html`
                    <rtl-ports
                      .ctx=${this.ctx}
                      .selectedPort=${port.portNum}
                      .embedded=${true}
                    ></rtl-ports>
                  `
                : nothing
            }
          </div>
        </div>
      </section>
    `;
  }

  private renderConnectionList(selected: Port) {
    return html`
      <section
        class="connection-browser"
        aria-labelledby="connections-title"
      >
        <div class="connection-browser-heading">
          <h2 id="connections-title">Connections</h2>
          <span>${this.ctx.ports.length} ports</span>
        </div>
        <div class="connection-list">
          ${this.ctx.ports.map(
            (port) => html`
              <button
                class="connection-row"
                data-port=${port.portNum}
                aria-pressed=${selected.portNum === port.portNum}
                @click=${this.selectPort}
              >
                <span class="mono">${String(port.portNum).padStart(2, "0")}</span>
                <strong>${port.name || `Port ${port.portNum}`}</strong>
                ${renderLinkBadge(port)}
              </button>
            `,
          )}
        </div>
      </section>
    `;
  }

  render() {
    const { ports } = this.ctx;
    const selected = ports.find((port) => port.portNum === this.ctx.selectedPort) ?? ports[0];
    const vlan = this.networks.find((network) => network.id === this.selectedVlan);

    return html`
      <section
        class="switch-workbench"
        aria-label="Switch ports"
      >
        <div class="workbench-toolbar">${this.renderOverlayPicker()}</div>
        ${renderDeviceFront({
          ports,
          selectedPort: selected?.portNum ?? 0,
          overlay: this.overlay,
          vlan: this.networksLoading || this.networksError ? undefined : vlan,
          onSelect: this.selectPort,
        })}
        <div class="workbench-networks">
          <span class="eyebrow">HIGHLIGHT NETWORK</span>
          ${this.renderNetworkPicker()}
        </div>
      </section>
      ${
        selected
          ? html`
              <div class="switch-details">
                ${this.renderConnectionList(selected)} ${this.renderPortDetails(selected)}
              </div>
            `
          : html`
              <p class="empty">No ports reported by the switch.</p>
            `
      }
    `;
  }
}

customElements.define("rtl-overview", OverviewPage);
