import { keyed } from "lit/directives/keyed.js";
import {
  Page,
  html,
  nothing,
  input,
  select,
  check,
  submit,
  heading,
  chip,
  portLabel,
  refreshButton,
  api,
} from "../shared";
import { hex, speed, type Port } from "../api";
import {
  portCommands,
  bandwidthCommands,
  type BandwidthState,
  type PortDraft,
} from "../domain/ports";
import { renderPortWorkbench } from "../components/port-workbench";
import type { PortAnnotation } from "../components/device-front";
import { renderPortActions } from "../components/port-actions";

interface MtuState {
  portNum: number;
  mtu: string;
}

interface EnergyState {
  portNum: number;
  isSFP: number;
  eee: string;
  eee_lp: string;
  active: number;
}

const COPPER_SPEEDS: [string, string][] = [
  ["keep", "Keep current setting"],
  ["auto", "Auto-negotiate"],
  ["2g5", "2.5 Gbps"],
  ["1g", "1 Gbps"],
  ["100m full", "100 Mbps · full duplex"],
  ["100m half", "100 Mbps · half duplex"],
  ["10m full", "10 Mbps · full duplex"],
  ["10m half", "10 Mbps · half duplex"],
];

export class PortsPage extends Page {
  static properties = {
    ...Page.properties,
    mode: {},
    embedded: { type: Boolean },
    selectedPort: { state: true },
    portAction: { state: true },
    energyDrafts: { state: true },
  };

  mode = "ports";
  embedded = false;
  selectedPort = 0;
  private mtus: MtuState[] = [];
  private bandwidth: BandwidthState[] = [];
  private energy: EnergyState[] = [];
  private drafts = new Map<number, PortDraft>();
  private portAction = "inspect";
  private energyDrafts = new Map<number, boolean>();

  get hasPendingChanges() {
    if (super.hasPendingChanges) return true;
    if (
      this.energy.some(
        (port) =>
          this.energyDrafts.has(port.portNum) &&
          this.energyDrafts.get(port.portNum) !== Boolean(parseInt(port.eee, 2)),
      )
    )
      return true;
    return this.ctx.ports.some((port) => {
      const draft = this.drafts.get(port.portNum);
      if (!draft) return false;
      return (
        draft.name !== port.name ||
        draft.enabled !== Boolean(port.enabled) ||
        draft.speed !== "keep" ||
        draft.mtu !== hex(this.mtus.find((item) => item.portNum === port.portNum)?.mtu)
      );
    });
  }

  private selectPortAction(event: Event) {
    this.portAction = (event.currentTarget as HTMLButtonElement).dataset.action!;
  }

  private toggleEnabled = (event: Event) => {
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const port = this.ctx.ports.find((item) => item.portNum === number);
    if (!port) return;
    const draft = this.drafts.get(number) ?? {
      name: port.name,
      enabled: Boolean(port.enabled),
      speed: "keep",
      mtu: hex(this.mtus.find((item) => item.portNum === number)?.mtu),
    };
    this.drafts.set(number, { ...draft, enabled: !draft.enabled });
    this.ctx.selectPort(number);
    this.revision_++;
  };

  private editBandwidth = async (event: Event) => {
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    this.selectPhysicalPort(event);
    this.ctx.changed();
    await this.updateComplete;
    requestAnimationFrame(() => {
      this.querySelector<HTMLInputElement>(
        `form[data-bandwidth="${number}"] input[name="ingress"]`,
      )?.focus();
    });
  };

  private clickPort = (event: Event) => {
    this.selectPhysicalPort(event);
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const port = this.ctx.ports.find((item) => item.portNum === number);
    if (!port) return;
    if (this.mode === "eee") {
      if (port.isSFP) return;
      const state = this.energy.find((item) => item.portNum === number);
      if (!state) return;
      const enabled = this.energyDrafts.get(number) ?? Boolean(parseInt(state.eee, 2));
      this.energyDrafts = new Map(this.energyDrafts).set(number, !enabled);
    } else if (this.mode === "ports" && this.portAction !== "inspect") {
      const draft = this.drafts.get(number) ?? {
        name: port.name,
        enabled: Boolean(port.enabled),
        speed: "keep",
        mtu: hex(this.mtus.find((item) => item.portNum === number)?.mtu),
      };
      this.drafts.set(number, { ...draft, enabled: this.portAction === "enable" });
      this.requestUpdate();
    }
  };

  private reviewPortDrafts() {
    if (this.querySelector<HTMLFormElement>("form")?.reportValidity() === false) return;
    try {
      const commands = this.ctx.ports.flatMap((port) => {
        const draft = this.drafts.get(port.portNum);
        if (!draft) return [];
        return portCommands(
          port,
          hex(this.mtus.find((item) => item.portNum === port.portNum)?.mtu),
          draft,
        );
      });
      if (!commands.length) return;
      this.ctx.review("Update port settings", commands, async () => {
        this.drafts.clear();
        await this.reload();
      });
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  async load() {
    if (!this.embedded && this.mode === "ports") {
      const query = new URLSearchParams(location.hash.split("?")[1]);
      const requestedPort = Number(query.get("port"));
      if (this.ctx.ports.some((port) => port.portNum === requestedPort))
        this.ctx.selectPort(requestedPort);
    }
    switch (this.mode) {
      case "ports":
        this.mtus = await api.json("/mtu.json");
        break;
      case "bandwidth":
        this.bandwidth = await api.json("/bandwidth.json");
        break;
      case "eee":
        this.energy = await api.json("/eee.json");
        break;
    }
  }

  private get editingPort() {
    return this.embedded ? this.selectedPort : (this.activePort?.portNum ?? 0);
  }

  private readPortDraft(form: FormData): PortDraft {
    return {
      name: this.value(form, "name"),
      enabled: form.has("enabled"),
      speed: this.value(form, "speed"),
      mtu: Number(form.get("mtu")),
    };
  }

  private handleDraftInput(event: Event) {
    const form = new FormData(event.currentTarget as HTMLFormElement);
    this.drafts.set(this.editingPort, this.readPortDraft(form));
  }

  private discardDraft() {
    this.drafts.delete(this.editingPort);
    this.revision_++;
  }

  private handlePortSubmit(event: Event) {
    const form = this.form(event);
    if (!this.embedded) {
      this.drafts.set(this.editingPort, this.readPortDraft(form));
      this.reviewPortDrafts();
      return;
    }
    const port = this.ctx.ports.find((item) => item.portNum === this.editingPort);
    if (!port) return;

    try {
      const currentMtu = hex(this.mtus.find((item) => item.portNum === port.portNum)?.mtu);
      const commands = portCommands(port, currentMtu, this.readPortDraft(form));
      if (!commands.length) {
        this.ctx.notify("No port settings changed.");
        return;
      }
      this.ctx.review(`Update ${portLabel(port)}`, commands, async () => {
        this.drafts.delete(port.portNum);
        await this.reload();
      });
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private handleBandwidthSubmit(event: Event) {
    const form = this.form(event);
    const current = this.bandwidth.find((item) => item.portNum === Number(form.get("port")));
    if (!current) return;

    try {
      const commands = bandwidthCommands(current, {
        ingress: Number(form.get("ingress")),
        egress: Number(form.get("egress")),
        flowControl: form.has("flow-control"),
      });
      this.review(`Bandwidth · port ${current.portNum}`, commands);
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private reviewEnergy() {
    const commands = this.energy.flatMap((port) => {
      const draft = this.energyDrafts.get(port.portNum);
      if (port.isSFP || draft === undefined || draft === Boolean(parseInt(port.eee, 2))) return [];
      return [`eee ${port.portNum} ${draft ? "on" : "off"}`];
    });
    if (!commands.length) return;
    this.ctx.review("Energy saving", commands, async () => {
      this.energyDrafts = new Map();
      await this.reload();
    });
  }

  private discardEnergy() {
    this.energyDrafts = new Map();
  }

  private enableAllEnergy() {
    this.energyDrafts = new Map(
      this.energy.filter((port) => !port.isSFP).map((port) => [port.portNum, true]),
    );
  }

  private disableAllEnergy() {
    this.energyDrafts = new Map(
      this.energy.filter((port) => !port.isSFP).map((port) => [port.portNum, false]),
    );
  }

  private renderPortEditor(port: Port) {
    const mtu = hex(this.mtus.find((item) => item.portNum === port.portNum)?.mtu);
    const draft = this.drafts.get(port.portNum) ?? {
      name: port.name,
      enabled: Boolean(port.enabled),
      speed: "keep",
      mtu,
    };
    const speeds = [...COPPER_SPEEDS];
    if (port.isSFP) speeds.splice(2, 0, ["10g", "10 Gbps"], ["5g", "5 Gbps"]);

    return html`
      <form
        class="card padded"
        @submit=${this.handlePortSubmit}
        @input=${this.handleDraftInput}
      >
        <div class="form-grid">
          <label class="field">
            <span>Port name</span>
            <input
              name="name"
              .value=${draft.name}
              maxlength="15"
              pattern="[A-Za-z0-9_.-]+"
            />
          </label>
          ${select("speed", draft.speed, "Advertised speed", speeds)}
          ${input("mtu", draft.mtu, "Maximum frame size (bytes)", { type: "number", min: 64, max: 16383 })}
          ${
            this.embedded
              ? check("enabled", draft.enabled, "Port enabled")
              : html`
                  <input
                    type="hidden"
                    name="enabled"
                    value="on"
                    ?disabled=${!draft.enabled}
                  />
                  <p>Port ${draft.enabled ? "enabled" : "disabled"}</p>
                `
          }
        </div>
        <div class="form-actions">
          <span class="muted">Current link: ${speed(port)}</span>
          <button
            type="button"
            @click=${this.discardDraft}
          >
            Discard draft
          </button>
          ${this.embedded ? submit() : nothing}
        </div>
        ${port.isSFP ? this.renderOptics(port) : nothing}
      </form>
    `;
  }

  private renderOptics(port: Port) {
    const readings = Object.entries(port).filter(([key]) => key.startsWith("sfp_"));
    return html`
      <details>
        <summary>Optical module diagnostics</summary>
        <dl class="reading-grid">
          ${readings.map(
            ([key, value]) => html`
              <div>
                <dt>${key.replace("sfp_", "").replaceAll("_", " ")}</dt>
                <dd>${String(value ?? "Not reported")}</dd>
              </div>
            `,
          )}
        </dl>
      </details>
    `;
  }

  private renderBandwidthEditor(state: BandwidthState) {
    return html`
      <section class="card padded bandwidth-card">
        ${heading("Traffic limits", "")}
        <form
          data-guard
          data-bandwidth=${state.portNum}
          @submit=${this.handleBandwidthSubmit}
        >
          <input
            type="hidden"
            name="port"
            value=${state.portNum}
          />
          <div class="form-grid">
            ${input("ingress", state.iLimited ? hex(state.iBW) * 16 : 0, "Ingress (Kbit/s)", { type: "number", min: 0, max: 10_000_000, step: 16 })}
            ${input("egress", state.eLimited ? hex(state.eBW) * 16 : 0, "Egress (Kbit/s)", { type: "number", min: 0, max: 10_000_000, step: 16 })}
            ${check("flow-control", Boolean(state.iFC), "Use flow control instead of dropping packets")}
          </div>
          <div class="form-actions">${submit()}</div>
        </form>
      </section>
    `;
  }

  private renderEnergyCapabilities(bits: string) {
    const mask = parseInt(bits, 2);
    return html`
      <div class="capabilities">
        ${[
          [4, "2.5G"],
          [2, "1G"],
          [1, "100M"],
        ].map(([bit, label]) => chip(String(label), Boolean(mask & Number(bit))))}
      </div>
    `;
  }

  private renderEnergyCard(port: EnergyState) {
    return html`
      <article class="card padded energy-card">
        <div class="toolbar">
          <h2>Port ${port.portNum}</h2>
          ${chip(port.active ? "Low-power mode" : "Ready", Boolean(port.active))}
        </div>
        <dl class="reading-grid">
          <div>
            <dt>Local capabilities</dt>
            <dd>${this.renderEnergyCapabilities(port.eee)}</dd>
          </div>
          <div>
            <dt>Link partner</dt>
            <dd>${this.renderEnergyCapabilities(port.eee_lp)}</dd>
          </div>
        </dl>
      </article>
    `;
  }

  protected render() {
    if (this.loading_ || this.error_) return this.state();
    const selected = this.ctx.ports.find((port) => port.portNum === this.editingPort);
    if (this.embedded)
      return selected
        ? keyed(`${selected.portNum}-${this.revision_}`, this.renderPortEditor(selected))
        : nothing;
    const titles: Record<string, string> = {
      ports: "Port settings",
      eee: "Energy saving",
      bandwidth: "Bandwidth",
    };
    return renderPortWorkbench({
      ctx: this.ctx,
      title: titles[this.mode],
      selected,
      onSelect: this.clickPort,
      toolbar: this.renderPortTools(),
      annotations: new Map(this.ctx.ports.map((port) => [port.portNum, this.portAnnotation(port)])),
      sidebar: this.renderContext(),
      inspector: this.renderSelectedSettings(selected),
    });
  }

  private portAnnotation(port: Port): PortAnnotation {
    const annotation = this.portStateAnnotation(port);
    if (
      this.mode === "eee" &&
      !port.isSFP &&
      this.energy.some((item) => item.portNum === port.portNum)
    ) {
      const state = this.energy.find((item) => item.portNum === port.portNum)!;
      const enabled = this.energyDrafts.get(port.portNum) ?? Boolean(parseInt(state.eee, 2));
      return {
        ...annotation,
        onActivate: this.clickPort,
        actionLabel: `Port ${port.portNum}: ${enabled ? "disable" : "enable"} EEE`,
      };
    }
    if (this.mode === "bandwidth")
      return {
        ...annotation,
        onActivate: this.editBandwidth,
        actionLabel: `Port ${port.portNum}: edit bandwidth limits`,
      };
    if (this.mode === "ports") {
      const enabled = this.drafts.get(port.portNum)?.enabled ?? Boolean(port.enabled);
      return {
        ...annotation,
        onActivate: this.toggleEnabled,
        actionLabel: `Port ${port.portNum}: ${enabled ? "disable" : "enable"} port`,
      };
    }
    return annotation;
  }

  private portStateAnnotation(port: Port): PortAnnotation {
    if (this.mode === "eee") {
      if (port.isSFP) return { label: "Not supported" };
      const state = this.energy.find((item) => item.portNum === port.portNum);
      if (!state) return { label: "Unknown" };
      const draft = this.energyDrafts.get(port.portNum);
      if (draft !== undefined && draft !== Boolean(parseInt(state.eee, 2)))
        return { label: draft ? "EEE enabled" : "EEE off", tone: "active", detail: "Draft" };
      if (state.active) return { label: "Low power", tone: "active" };
      if (parseInt(state.eee, 2)) return { label: "EEE enabled", tone: "active" };
      return { label: "EEE off" };
    }
    if (this.mode === "bandwidth") {
      const state = this.bandwidth.find((item) => item.portNum === port.portNum);
      if (!state) return { label: "Unknown" };
      if (state.iLimited || state.eLimited) return { label: "Limited", tone: "warning" };
      return { label: "Unlimited" };
    }
    const draft = this.drafts.get(port.portNum);
    if (draft && draft.enabled !== Boolean(port.enabled))
      return {
        label: draft.enabled ? "Enabled" : "Disabled",
        tone: "warning",
        detail: "Draft",
      };
    const mtu = this.mtus.find((item) => item.portNum === port.portNum);
    return {
      label: port.enabled ? "Enabled" : "Disabled",
      detail: mtu ? `MTU ${hex(mtu.mtu)}` : "MTU unknown",
    };
  }

  private renderPortTools() {
    if (this.mode === "eee")
      return html`
        <div class="form-actions">
          <button
            type="button"
            @click=${this.discardEnergy}
          >
            Discard draft
          </button>
          <button
            type="button"
            class="primary"
            ?disabled=${!this.energyDrafts.size}
            @click=${this.reviewEnergy}
          >
            Review changes
          </button>
        </div>
      `;
    if (this.mode === "ports")
      return html`
        ${renderPortActions(
          this.portAction,
          [
            ["inspect", "Edit settings"],
            ["enable", "Enable port"],
            ["disable", "Disable port"],
          ],
          this.selectPortAction,
        )}
        <div class="form-actions">
          <button
            type="button"
            class="primary"
            @click=${this.reviewPortDrafts}
          >
            Review changes
          </button>
        </div>
      `;
    return undefined;
  }

  private renderContext() {
    if (this.mode === "eee")
      return html`
        <h2>Energy Efficient Ethernet</h2>
        <h3>All copper ports</h3>
        <div class="port-choice-actions">
          <button @click=${this.enableAllEnergy}>Enable on all ports</button>
          <button @click=${this.disableAllEnergy}>Disable on all ports</button>
        </div>
        ${refreshButton(this)}
      `;
    if (this.mode === "bandwidth")
      return html`
        <h2>Bandwidth limits</h2>
        <dl class="reading-grid">
          <div>
            <dt>Rate step</dt>
            <dd>16 Kbit/s</dd>
          </div>
          <div>
            <dt>Unlimited</dt>
            <dd>0 Kbit/s</dd>
          </div>
        </dl>
        ${refreshButton(this)}
      `;
    return html`
      <h2>Connection settings</h2>
      <dl class="reading-grid">
        <div>
          <dt>Enabled</dt>
          <dd>${this.ctx.ports.filter((port) => port.enabled).length}</dd>
        </div>
        <div>
          <dt>Disabled</dt>
          <dd>${this.ctx.ports.filter((port) => !port.enabled).length}</dd>
        </div>
      </dl>
      ${refreshButton(this)}
    `;
  }

  private renderSelectedSettings(selected?: Port) {
    if (!selected)
      return html`
        <p>No ports reported.</p>
      `;
    if (this.mode === "bandwidth")
      return html`
        ${this.bandwidth.map(
          (state) => html`
            <div ?hidden=${state.portNum !== selected.portNum}>
              ${this.renderBandwidthEditor(state)}
            </div>
          `,
        )}
      `;
    if (this.mode === "eee") {
      if (selected.isSFP)
        return html`
          <p>EEE is not supported on the SFP+ interface.</p>
        `;
      const state = this.energy.find((item) => item.portNum === selected.portNum);
      return state
        ? this.renderEnergyCard(state)
        : html`
            <p>Energy state unavailable.</p>
          `;
    }
    return html`
      ${keyed(`${selected.portNum}-${this.revision_}`, this.renderPortEditor(selected))}
    `;
  }
}

customElements.define("rtl-ports", PortsPage);
