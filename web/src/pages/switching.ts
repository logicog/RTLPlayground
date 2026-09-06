import { Page, html, input, select, check, heading, chip, refreshButton, api } from "../shared";
import { hex, type Port } from "../api";
import { renderPortWorkbench } from "../components/port-workbench";
import type { PortAnnotation } from "../components/device-front";
import { renderPortActions } from "../components/port-actions";
import { aggregationCommands, toggleAggregationPort } from "../domain/aggregation";
import { entityColor } from "../domain/entity-colors";
import {
  editMirrorPort,
  mirrorCommand,
  type MirrorSettings,
  type MirrorAction,
} from "../domain/port-actions";
import {
  bridgeCommands,
  portSettings,
  spanningPortCommands,
  type SpanningTree,
  type SpanningTreePort,
} from "../domain/spanning-tree";

interface AggregationGroup {
  members: string;
  hash: string;
}

export class SwitchingPage extends Page {
  static properties = {
    ...Page.properties,
    mode: {},
    selectedGroup: { state: true },
    groupDrafts: { state: true },
    mirrorDraft: { state: true },
    mirrorAction: { state: true },
    stpAction: { state: true },
    participationDrafts: { state: true },
  };
  mode = "lag";

  private groups: AggregationGroup[] = [];
  private mirror!: MirrorSettings;
  private bridge!: SpanningTree;
  private selectedGroup = 1;
  private groupDrafts = new Map<number, number>();
  private mirrorDraft?: MirrorSettings;
  private mirrorAction: MirrorAction = "both";
  private stpAction = "inspect";
  private participationDrafts = new Map<number, boolean>();

  get hasPendingChanges() {
    if (super.hasPendingChanges) return true;
    if (this.mode === "lag")
      return this.groups.some(
        (group, index) => this.groupMask(index) !== parseInt(group.members, 2),
      );
    if (this.mode === "mirror" && this.mirrorDraft) {
      return (
        this.mirrorDraft.enabled !== this.mirror.enabled ||
        this.mirrorDraft.mPort !== this.mirror.mPort ||
        parseInt(this.mirrorDraft.mirror_tx, 2) !== parseInt(this.mirror.mirror_tx, 2) ||
        parseInt(this.mirrorDraft.mirror_rx, 2) !== parseInt(this.mirror.mirror_rx, 2)
      );
    }
    return (
      this.bridge?.ports.some(
        (port) =>
          this.participationDrafts.has(port.p) &&
          this.participationDrafts.get(port.p) !== Boolean(port.f & 1),
      ) ?? false
    );
  }

  async load() {
    switch (this.mode) {
      case "lag":
        this.groups = await api.json("/lag.json");
        break;
      case "mirror":
        this.mirror = await api.json("/mirror.json");
        break;
      case "stp":
        this.bridge = await api.json("/stp.json");
        break;
    }
  }

  private selectGroup(event: Event) {
    this.selectedGroup = Number((event.currentTarget as HTMLButtonElement).dataset.group);
  }

  private toggleGroupPort = (event: Event) => {
    this.selectPhysicalPort(event);
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const port = this.ctx.ports.find((item) => item.portNum === number);
    if (!port) return;
    const masks = toggleAggregationPort(
      this.groups.map((_, index) => this.groupMask(index)),
      this.selectedGroup - 1,
      port.logPort,
    );
    this.groupDrafts = new Map(masks.map((mask, index) => [index + 1, mask]));
  };

  private submitGroup() {
    const commands = aggregationCommands(
      this.groups.map((group) => parseInt(group.members, 2)),
      this.groups.map((_, index) => this.groupMask(index)),
      this.ctx.ports,
    );
    if (!commands.length) return;
    this.ctx.review("Aggregation groups", commands, async () => {
      this.groupDrafts = new Map();
      await this.reload();
    });
  }

  private selectMirrorAction(event: Event) {
    this.mirrorAction = (event.currentTarget as HTMLButtonElement).dataset.action as MirrorAction;
  }

  private editMirror = (event: Event) => {
    this.selectPhysicalPort(event);
    if (this.mirrorAction === "inspect") return;
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const port = this.ctx.ports.find((item) => item.portNum === number);
    if (!port) return;
    try {
      this.mirrorDraft = editMirrorPort(this.mirrorDraft ?? this.mirror, port, this.mirrorAction);
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  };

  private submitMirror() {
    if (!this.mirrorDraft) return;
    try {
      const command = mirrorCommand(this.mirrorDraft, this.ctx.ports);
      if (command === mirrorCommand(this.mirror, this.ctx.ports)) return;
      this.ctx.review("Configure port mirroring", [command], async () => {
        this.mirrorDraft = undefined;
        await this.reload();
      });
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private discardDirectDraft() {
    if (this.mode === "lag") this.groupDrafts = new Map();
    else this.mirrorDraft = undefined;
  }

  private selectStpAction(event: Event) {
    this.stpAction = (event.currentTarget as HTMLButtonElement).dataset.action!;
  }

  private toggleParticipation = (event: Event) => {
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const port = this.bridge.ports.find((item) => item.p === number);
    if (!port) return;
    const enabled = this.participationDrafts.get(number) ?? Boolean(port.f & 1);
    this.participationDrafts = new Map(this.participationDrafts).set(number, !enabled);
    this.ctx.selectPort(number);
  };

  private cycleMirrorSource = (event: Event) => {
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const port = this.ctx.ports.find((item) => item.portNum === number);
    const state = this.mirrorDraft ?? this.mirror;
    if (!port || state.mPort === number) return;
    const bit = 1 << port.logPort;
    const tx = Boolean(parseInt(state.mirror_tx, 2) & bit);
    const rx = Boolean(parseInt(state.mirror_rx, 2) & bit);
    let action: MirrorAction = "rx";
    if (tx && rx) action = "remove";
    else if (tx) action = "both";
    else if (rx) action = "tx";
    this.mirrorDraft = editMirrorPort(state, port, action);
    this.ctx.selectPort(number);
  };

  private editParticipation = (event: Event) => {
    this.selectPhysicalPort(event);
    if (this.stpAction === "inspect") return;
    const number = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    this.participationDrafts = new Map(this.participationDrafts).set(
      number,
      this.stpAction === "enable",
    );
  };

  private reviewSpanningTree(event: Event) {
    event.preventDefault();
    try {
      const commands: string[] = [];
      for (const element of this.querySelectorAll<HTMLFormElement>("form[data-stp]")) {
        if (!element.checkValidity()) {
          const number = Number(element.dataset.port);
          if (number) this.ctx.selectPort(number);
          this.ctx.notify("Check the spanning-tree fields before reviewing.", true);
          return;
        }
        const form = new FormData(element);
        if (element.dataset.stp === "bridge") {
          commands.push(...bridgeCommands(this.bridge, form));
          continue;
        }
        const port = this.bridge.ports.find((item) => item.p === Number(element.dataset.port));
        if (!port) continue;
        form.set(
          "enabled",
          (this.participationDrafts.get(port.p) ?? Boolean(port.f & 1)) ? "on" : "off",
        );
        commands.push(...spanningPortCommands(port, form));
      }
      if (!commands.length) return;
      this.ctx.review("Spanning tree", commands, async () => {
        this.participationDrafts = new Map();
        await this.reload();
      });
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private renderGroup(index: number) {
    const members = this.ctx.ports.filter((port) => this.groupMask(index) & (1 << port.logPort));
    const changedGroups = this.groups.flatMap((item, groupIndex) =>
      this.groupMask(groupIndex) !== parseInt(item.members, 2) ? [groupIndex + 1] : [],
    );
    return html`
      <section>
        <h2
          class="group-heading entity-color"
          style=${entityColor(index + 1)}
        >
          <span
            class="network-swatch"
            aria-hidden="true"
          ></span>
          Group ${index + 1}
        </h2>
        <p>
          ${members.length ? `Member ports: ${members.map((port) => port.portNum).join(", ")}` : "No member ports"}
        </p>
        <p class="field-help">
          ${changedGroups.length ? `Draft · groups ${changedGroups.join(", ")}` : "No pending changes"}
        </p>
        <div class="form-actions">
          <button
            type="button"
            @click=${this.discardDirectDraft}
          >
            Discard draft
          </button>
          <button
            type="button"
            class="primary"
            ?disabled=${!changedGroups.length}
            @click=${this.submitGroup}
          >
            Review changes
          </button>
        </div>
      </section>
    `;
  }

  private renderMirroring() {
    const state = this.mirrorDraft ?? this.mirror;
    return renderPortWorkbench({
      ctx: this.ctx,
      title: "Mirroring",
      selected: this.activePort,
      onSelect: this.editMirror,
      annotations: this.annotations(),
      toolbar: renderPortActions(
        this.mirrorAction,
        [
          ["both", "Copy RX + TX"],
          ["rx", "Copy RX"],
          ["tx", "Copy TX"],
          ["destination", "Set destination"],
          ["remove", "Remove source"],
          ["inspect", "Inspect"],
        ],
        this.selectMirrorAction,
      ),
      sidebar: html`
        <h2>Monitoring destination</h2>
        <p>Port ${state.mPort} · ${state.enabled ? "Mirroring enabled" : "Mirroring off"}</p>
      `,
      inspector: html`
        <h2>${this.activePort ? this.portAnnotation(this.activePort).label : "Port mirroring"}</h2>
        <p class="field-help">${this.mirrorDraft ? "Draft" : "No pending changes"}</p>
        <div class="form-actions">
          <button
            type="button"
            @click=${this.discardDirectDraft}
          >
            Discard draft
          </button>
          <button
            type="button"
            class="primary"
            ?disabled=${!this.mirrorDraft}
            @click=${this.submitMirror}
          >
            Review changes
          </button>
        </div>
      `,
    });
  }

  private renderBridgeSettings() {
    const bridge = this.bridge;
    const priorities = Array.from(
      { length: 16 },
      (_, index) => [index, String(index * 4096)] as [number, string],
    );
    return html`
      <form
        class="card padded"
        data-stp="bridge"
        data-guard
        @submit=${this.reviewSpanningTree}
      >
        ${heading("Bridge settings", "")}
        <div class="form-grid">
          ${check("on", Boolean(bridge.on), "Enable spanning tree")}
          ${select("version", bridge.rstp ? "rstp" : "stp", "Protocol", [
            ["rstp", "Rapid STP"],
            ["stp", "Classic STP"],
          ])}
          ${select("prio", bridge.prio, "Bridge priority", priorities)}
          ${input("hello", bridge.hello, "Hello time (s)", { type: "number", min: 1, max: 10 })}
          ${input("maxage", bridge.maxage, "Maximum age (s)", { type: "number", min: 6, max: 40 })}
          ${input("fwd", bridge.fwd, "Forward delay (s)", { type: "number", min: 4, max: 30 })}
          ${input("txhold", bridge.txhold, "Transmit hold count", { type: "number", min: 1, max: 10 })}
        </div>
      </form>
    `;
  }

  private renderBridgePort(port: SpanningTreePort) {
    const settings = portSettings(port);
    const priorityOptions = Array.from(
      { length: 16 },
      (_, index) => [index * 16, String(index * 16)] as [number, string],
    );
    const state = ["Disabled", "Blocking", "Learning", "Forwarding"][port.st];
    const role = ["—", "Root", "Designated", "Alternate"][port.role];

    return html`
      <section class="card padded">
        <div class="toolbar">
          <strong>Port ${port.p}</strong>
          ${chip(this.bridge.on ? state : "STP off", Boolean(this.bridge.on && port.st === 3))}
          <span class="muted">${role} ${port.f & 128 ? "· Guard triggered" : ""}</span>
        </div>
        <form
          data-stp="port"
          data-guard
          data-port=${port.p}
          @submit=${this.reviewSpanningTree}
        >
          <input
            type="hidden"
            name="port"
            value=${port.p}
          />
          <div class="form-grid">
            <p>
              Participation:
              ${(this.participationDrafts.get(port.p) ?? Boolean(port.f & 1)) ? "On" : "Off"}
            </p>
            ${input("cost", settings.cost, "Path cost (0 = automatic)", { type: "number", min: 0, max: 200_000_000 })}
            ${select("prio", settings.prio, "Port priority", priorityOptions)}
            ${select("edge", settings.edge, "Edge port", ["auto", "on", "off"])}
            ${select("guard", settings.guard, "Guard", ["none", "bpdu", "root"])}
            ${select("filter", settings.filter, "BPDU filter", ["off", "on"])}
            ${select("p2p", settings.p2p, "Point to point", ["auto", "on", "off"])}
          </div>
          <dl class="reading-grid">
            <div>
              <dt>Designated bridge</dt>
              <dd class="mono">${port.db}</dd>
            </div>
            <div>
              <dt>Designated port</dt>
              <dd>${port.dp}</dd>
            </div>
            <div>
              <dt>Designated cost</dt>
              <dd>${hex(port.dc)}</dd>
            </div>
            <div>
              <dt>Operational edge</dt>
              <dd>${port.f & 64 ? "Yes" : "No"}</dd>
            </div>
          </dl>
        </form>
      </section>
    `;
  }

  private renderSpanningTree() {
    let protocol = "Disabled";
    if (this.bridge.on) protocol = this.bridge.rstp ? "RSTP" : "STP";
    return renderPortWorkbench({
      ctx: this.ctx,
      title: "Spanning tree",
      selected: this.activePort,
      onSelect: this.editParticipation,
      toolbar: html`
        ${renderPortActions(
          this.stpAction,
          [
            ["inspect", "Inspect"],
            ["enable", "Enable participation"],
            ["disable", "Disable participation"],
          ],
          this.selectStpAction,
        )}
        <div class="form-actions">
          <button
            type="button"
            class="primary"
            @click=${this.reviewSpanningTree}
          >
            Review changes
          </button>
        </div>
      `,
      annotations: this.annotations(),
      sidebar: html`
        <h2>Bridge & topology</h2>
        <dl class="reading-grid">
          <div>
            <dt>Protocol</dt>
            <dd>${protocol}</dd>
          </div>
          <div>
            <dt>Root bridge</dt>
            <dd>${this.bridge.weRoot ? "This switch" : `Port ${this.bridge.rootPort}`}</dd>
          </div>
          <div>
            <dt>Topology changes</dt>
            <dd>${hex(this.bridge.tc)}</dd>
          </div>
        </dl>
        ${refreshButton(this)}
        <details>
          <summary>Bridge settings</summary>
          <p class="notice">
            Enabling STP initially blocks ports. Forwarding can take up to ${2 * this.bridge.fwd}
            seconds.
          </p>
          ${this.renderBridgeSettings()}
        </details>
      `,
      inspector: html`
        ${this.bridge.ports.map(
          (port) => html`
            <div ?hidden=${port.p !== this.activePort?.portNum}>${this.renderBridgePort(port)}</div>
          `,
        )}
      `,
    });
  }

  protected render() {
    if (this.loading_ || this.error_) return this.state();
    if (this.mode === "mirror") return this.renderMirroring();
    if (this.mode === "stp") return this.renderSpanningTree();
    return renderPortWorkbench({
      ctx: this.ctx,
      title: "Aggregation",
      selected: this.activePort,
      showPortHeading: false,
      onSelect: this.toggleGroupPort,
      selectedPorts: new Set(
        this.ctx.ports
          .filter((port) => this.groupMask(this.selectedGroup - 1) & (1 << port.logPort))
          .map((port) => port.portNum),
      ),
      annotations: this.annotations(),
      toolbar: html`
        <div
          class="network-selector"
          role="group"
          aria-label="Aggregation group"
        >
          ${this.groups.map(
            (group, index) => html`
              <button
                type="button"
                class="network-choice entity-color"
                style=${entityColor(index + 1)}
                data-group=${index + 1}
                aria-pressed=${this.selectedGroup === index + 1}
                @click=${this.selectGroup}
              >
                <span
                  class="network-swatch"
                  aria-hidden="true"
                ></span>
                <strong>Group ${index + 1}</strong>
                <span>${this.groupMemberCount(index)} ports</span>
              </button>
            `,
          )}
        </div>
      `,
      sidebar: html`
        <h2>Static link aggregation</h2>
        <p>Peer must use static aggregation.</p>
        ${refreshButton(this)}
      `,
      inspector: html`
        ${this.groups.map(
          (group, index) => html`
            <div ?hidden=${index + 1 !== this.selectedGroup}>${this.renderGroup(index)}</div>
          `,
        )}
      `,
    });
  }

  private groupMask(index: number) {
    return this.groupDrafts.get(index + 1) ?? parseInt(this.groups[index].members, 2);
  }

  private groupMemberCount(index: number) {
    return this.ctx.ports.filter((port) => this.groupMask(index) & (1 << port.logPort)).length;
  }

  private annotations(): Map<number, PortAnnotation> {
    return new Map(this.ctx.ports.map((port) => [port.portNum, this.portAnnotation(port)]));
  }

  private portAnnotation(port: Port): PortAnnotation {
    const annotation = this.portStateAnnotation(port);
    if (this.mode === "lag") return annotation;
    if (this.mode === "mirror") {
      const state = this.mirrorDraft ?? this.mirror;
      if (state.mPort === port.portNum) return annotation;
      return {
        ...annotation,
        onActivate: this.cycleMirrorSource,
        actionLabel: `Port ${port.portNum}: change mirror direction`,
      };
    }
    const state = this.bridge.ports.find((item) => item.p === port.portNum);
    if (!state) return annotation;
    const enabled = this.participationDrafts.get(port.portNum) ?? Boolean(state.f & 1);
    return {
      ...annotation,
      label: enabled ? "Participating" : "Excluded",
      detail: annotation.detail === "Draft" ? "Draft" : annotation.label,
      onActivate: this.toggleParticipation,
      actionLabel: `Port ${port.portNum}: ${enabled ? "disable" : "enable"} STP participation`,
    };
  }

  private portStateAnnotation(port: Port): PortAnnotation {
    if (this.mode === "lag") {
      const groups = this.groups.flatMap((_, index) =>
        this.groupMask(index) & (1 << port.logPort) ? [index + 1] : [],
      );
      if (groups.length > 1) return { label: "Conflict", tone: "warning" };
      if (groups.length)
        return { label: `Group ${groups[0]}`, tone: "active", entityId: groups[0] };
      return { label: "Standalone" };
    }
    if (this.mode === "mirror") {
      const state = this.mirrorDraft ?? this.mirror;
      if (!state.enabled) return { label: "Mirror off" };
      if (state.mPort === port.portNum) return { label: "Destination", tone: "active" };
      const tx = Boolean(parseInt(state.mirror_tx, 2) & (1 << port.logPort));
      const rx = Boolean(parseInt(state.mirror_rx, 2) & (1 << port.logPort));
      if (tx && rx) return { label: "RX + TX", tone: "active" };
      if (tx || rx) return { label: tx ? "TX source" : "RX source", tone: "active" };
      return { label: "Not mirrored" };
    }
    if (this.participationDrafts.has(port.portNum))
      return {
        label: this.participationDrafts.get(port.portNum) ? "Participating" : "Excluded",
        tone: "active",
        detail: "Draft",
      };
    if (!this.bridge.on) return { label: "STP off" };
    const state = this.bridge.ports.find((item) => item.p === port.portNum);
    if (!state) return { label: "Unknown" };
    if (state.f & 128) return { label: "Guard triggered", tone: "warning" };
    return {
      label: ["Disabled", "Blocking", "Learning", "Forwarding"][state.st] ?? "Unknown",
      tone: state.st === 3 ? "active" : "warning",
      detail: ["", "Root", "Designated", "Alternate"][state.role],
    };
  }
}

customElements.define("rtl-switching", SwitchingPage);
