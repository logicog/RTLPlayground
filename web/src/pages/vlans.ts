import { Page, html, nothing, api } from "../shared";
import { hex, type Port, type VlanList } from "../api";
import { renderDeviceFront, renderLinkBadge } from "../components/device-front";
import { renderVlanBulkActions } from "../components/vlan-bulk-actions";
import { entityColor } from "../domain/entity-colors";
import { decodeVlan, networkName, type VlanDetails } from "../domain/port-networks";
import {
  createVlanDraft,
  previewVlan,
  vlanChanges,
  vlanCommands,
  validateVlanNameBudget,
  setVlanMembership,
  nextMembership,
  type PortMembership,
  type VlanDraft,
} from "../domain/vlans";
import "./vlans.css";

interface VlanEdit {
  original: VlanDetails;
  draft: VlanDraft;
}

const membershipOptions: { value: PortMembership; label: string; description: string }[] = [
  { value: "none", label: "Excluded", description: "This VLAN is not carried on the port." },
  { value: "tagged", label: "Tagged", description: "Keep the VLAN tag on outgoing frames." },
  {
    value: "untagged",
    label: "Untagged",
    description: "Remove the VLAN tag from outgoing frames.",
  },
];

export class VlansPage extends Page {
  static properties = {
    ...Page.properties,
    selectedId: { state: true },
    draftRevision: { state: true },
    reviewing: { state: true },
    bulkMode: { state: true },
    selectedPorts: { state: true },
  };

  private vlans: VlanList = { mgmt: 0, vlan: [] };
  private edits = new Map<number, VlanEdit>();
  private selectedId: number | null = null;
  private draftRevision = 0;
  private reviewing = false;
  private bulkMode = false;
  private selectedPorts = new Set<number>();

  get hasPendingChanges() {
    return [...this.edits.values()].some(
      (edit) => vlanChanges(edit.original, edit.draft, this.ctx.ports).length > 0,
    );
  }

  async load() {
    const vlans = await api.json<VlanList>("/vlanlist");
    const edits = new Map<number, VlanEdit>();
    for (const vlan of vlans.vlan) {
      if (!this.isConnected) return;
      const response = await api.json<Partial<VlanDetails>>(`/vlan.json?vid=${vlan.id}`);
      const original = decodeVlan(vlan, response);
      const previous = this.edits.get(vlan.id);
      const keepDraft =
        previous && vlanChanges(previous.original, previous.draft, this.ctx.ports).length;
      edits.set(
        vlan.id,
        keepDraft ? previous : { original, draft: createVlanDraft(original, this.ctx.ports) },
      );
    }
    const newVlan = this.edits.get(0);
    if (newVlan) edits.set(0, newVlan);
    this.vlans = vlans;
    this.edits = edits;
    if (this.selectedId === null || !edits.has(this.selectedId)) {
      this.selectedId = vlans.vlan[0]?.id ?? null;
    }
  }

  private get currentEdit() {
    return this.selectedId === null ? undefined : this.edits.get(this.selectedId);
  }

  private createVlan() {
    if (!this.edits.has(0)) {
      const original = { id: 0, name: "", members: "0", pvid: "0" };
      this.edits.set(0, { original, draft: createVlanDraft(original, this.ctx.ports) });
    }
    this.selectedId = 0;
  }

  private selectVlan(event: Event) {
    this.selectedId = Number((event.currentTarget as HTMLButtonElement).dataset.vlan);
  }

  private selectPort = (event: Event) => {
    const button = event.currentTarget as HTMLButtonElement;
    const port = Number(button.dataset.port);
    if (this.bulkMode) {
      const selected = new Set(this.selectedPorts);
      if (selected.has(port)) selected.delete(port);
      else selected.add(port);
      this.selectedPorts = selected;
      return;
    }
    this.ctx.selectPort(port);
  };

  private toggleBulkMode(event: Event) {
    this.bulkMode = (event.currentTarget as HTMLInputElement).checked;
    this.selectedPorts = new Set();
  }

  private cyclePortMembership = (event: Event) => {
    const edit = this.currentEdit;
    if (!edit || this.reviewing || this.bulkMode) return;
    const port = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    const state = edit.draft.ports[port];
    if (!state) return;
    const next = nextMembership(state);
    edit.draft = setVlanMembership(edit.draft, [port], next);
    this.draftRevision++;
    this.ctx.selectPort(port);
  };

  private selectAllPorts() {
    this.selectedPorts = new Set(this.ctx.ports.map((port) => port.portNum));
  }

  private clearPortSelection() {
    this.selectedPorts = new Set();
  }

  private get bulkTargets(): number[] {
    if (this.bulkMode)
      return this.ctx.ports
        .filter((port) => this.selectedPorts.has(port.portNum))
        .map((port) => port.portNum);
    return this.ctx.ports.map((port) => port.portNum);
  }

  private changeBulkMembership(event: Event) {
    const edit = this.currentEdit;
    if (!edit || this.reviewing) return;
    const membership = (event.currentTarget as HTMLButtonElement).dataset
      .membership as PortMembership;
    try {
      edit.draft = setVlanMembership(edit.draft, this.bulkTargets, membership);
      this.draftRevision++;
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private renderBulkActions(edit: VlanEdit) {
    return renderVlanBulkActions({
      multiple: this.bulkMode,
      total: this.ctx.ports.length,
      selected: this.bulkTargets.length,
      reviewing: this.reviewing,
      canExclude: this.bulkTargets.every((port) => !edit.draft.ports[port].pvid),
      onToggle: this.toggleBulkMode,
      onSelectAll: this.selectAllPorts,
      onClear: this.clearPortSelection,
      onMembership: this.changeBulkMembership,
    });
  }

  private renderSelectionSummary() {
    return html`
      <section class="vlan-selection-summary">
        <h2>${this.selectedPorts.size} ports selected</h2>
        <p>
          Click ports on the switch to select or deselect them. Use Tagged, Untagged or Excluded
          above to update the selection.
        </p>
        <p>PVID is configured separately for each port in the single-port view.</p>
      </section>
    `;
  }

  private changeIdentity(event: Event) {
    const field = event.currentTarget as HTMLInputElement;
    const edit = this.currentEdit;
    if (!edit) return;
    if (field.name === "vid") edit.draft = { ...edit.draft, id: Number(field.value) };
    else edit.draft = { ...edit.draft, name: field.value };
    this.draftRevision++;
  }

  private changeMembership(event: Event) {
    const field = event.currentTarget as HTMLInputElement;
    const edit = this.currentEdit;
    if (!edit || this.reviewing) return;
    try {
      edit.draft = setVlanMembership(
        edit.draft,
        [Number(field.dataset.port)],
        field.value as PortMembership,
      );
      this.draftRevision++;
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private changePvid(event: Event) {
    const field = event.currentTarget as HTMLInputElement;
    this.updatePortDraft(Number(field.dataset.port), { pvid: field.checked });
  }

  private updatePortDraft(port: number, update: Partial<VlanDraft["ports"][number]>) {
    const edit = this.currentEdit;
    if (!edit) return;
    edit.draft = {
      ...edit.draft,
      ports: { ...edit.draft.ports, [port]: { ...edit.draft.ports[port], ...update } },
    };
    this.draftRevision++;
  }

  private discardDraft() {
    const edit = this.currentEdit;
    if (!edit) return;
    if (!edit.original.id) {
      this.edits.delete(0);
      this.selectedId = this.vlans.vlan[0]?.id ?? null;
    } else {
      edit.draft = createVlanDraft(edit.original, this.ctx.ports);
    }
    this.draftRevision++;
  }

  private async checkCurrentState(edit: VlanEdit) {
    const current = await api.json<VlanList>("/vlanlist");
    if (edit.draft.name !== edit.original.name) validateVlanNameBudget(current.vlan, edit.draft);
    const vlan = current.vlan.find((item) => item.id === edit.draft.id);
    if (!edit.original.id) {
      if (vlan)
        throw new Error("This VLAN already exists. Choose another ID or edit that network.");
      return;
    }
    if (!vlan) throw new Error("This VLAN was removed. Refresh before making changes.");
    const response = await api.json<Partial<VlanDetails>>(`/vlan.json?vid=${vlan.id}`);
    const latest = decodeVlan(vlan, response);
    const changed =
      latest.name !== edit.original.name ||
      hex(latest.members) !== hex(edit.original.members) ||
      hex(latest.pvid) !== hex(edit.original.pvid);
    if (changed)
      throw new Error(
        "This VLAN changed since you started editing. Discard the draft and refresh to read its current state.",
      );
    return current;
  }

  private async finishEditing(id: number, selectedId = id) {
    this.edits.delete(id);
    this.selectedId = selectedId;
    await this.reload();
  }

  private async submitVlan(event: Event) {
    event.preventDefault();
    const edit = this.currentEdit;
    if (!edit || this.reviewing) return;
    const revision = this.draftRevision;
    this.reviewing = true;
    try {
      const commands = vlanCommands(edit.original, this.ctx.ports, edit.draft);
      if (!commands.length) return;
      await this.checkCurrentState(edit);
      if (revision !== this.draftRevision) throw new Error("The draft changed. Review it again.");
      const id = edit.draft.id;
      this.ctx.review(`Update VLAN ${id}`, commands, () =>
        this.finishEditing(edit.original.id, id),
      );
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    } finally {
      this.reviewing = false;
    }
  }

  private async deleteVlan() {
    const edit = this.currentEdit;
    if (!edit?.original.id || this.reviewing) return;
    this.reviewing = true;
    try {
      const current = await this.checkCurrentState(edit);
      if (current?.mgmt === edit.original.id || hex(edit.original.pvid)) {
        throw new Error("A management VLAN or a VLAN used as a port PVID cannot be deleted here.");
      }
      const id = edit.original.id;
      this.ctx.review(`Delete VLAN ${id}`, [`vlan ${id} d`], () => this.finishEditing(id));
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    } finally {
      this.reviewing = false;
    }
  }

  private renderNetworkPicker() {
    return html`
      <div class="vlan-navigation">
        <div
          class="network-selector"
          role="group"
          aria-label="Select a network"
        >
          ${[...this.edits].map(([id, edit]) => {
            const dirty = vlanChanges(edit.original, edit.draft, this.ctx.ports).length > 0;
            return html`
              <button
                type="button"
                class="network-choice entity-color"
                style=${entityColor(id)}
                data-vlan=${id}
                aria-pressed=${this.selectedId === id}
                @click=${this.selectVlan}
              >
                <span
                  class="network-swatch"
                  aria-hidden="true"
                ></span>
                <strong>${id ? networkName(edit.original) : "New VLAN"}</strong>
                <span>${id === this.vlans.mgmt ? "Management" : id || "Draft"}</span>
                ${
                  dirty
                    ? html`
                        <span class="draft-indicator">Draft</span>
                      `
                    : nothing
                }
              </button>
            `;
          })}
        </div>
        <button
          type="button"
          @click=${this.createVlan}
        >
          ＋ Create VLAN
        </button>
      </div>
    `;
  }

  private renderIdentity(edit: VlanEdit) {
    return html`
      <div class="vlan-identity">
        <label class="field vlan-id-field">
          <span>VLAN ID</span>
          <input
            name="vid"
            type="number"
            min="1"
            max="4094"
            required
            .value=${String(edit.draft.id || "")}
            ?readonly=${Boolean(edit.original.id)}
            @input=${this.changeIdentity}
          />
        </label>
        <label class="field">
          <span>Network name</span>
          <input
            name="name"
            maxlength="24"
            pattern="[A-Za-z][A-Za-z0-9_]*"
            .value=${edit.draft.name}
            @input=${this.changeIdentity}
          />
        </label>
        <div class="vlan-context">
          <strong>
            ${edit.original.id === this.vlans.mgmt ? "Management network" : "Port membership"}
          </strong>
        </div>
      </div>
    `;
  }

  private renderPortEditor(edit: VlanEdit, port: Port) {
    const currentPvid = [...this.edits.values()].find(
      (item) => hex(item.original.pvid) & (1 << port.logPort),
    );
    return html`
      <section
        class="port-inspector vlan-port-editor"
        aria-labelledby="vlan-port-title"
      >
        <div class="inspector-heading">
          <span class="port-index">${String(port.portNum).padStart(2, "0")}</span>
          <div>
            <p>PORT ${port.portNum} / VLAN ${edit.draft.id || "NEW"}</p>
            <h2 id="vlan-port-title">${port.name || `Port ${port.portNum}`}</h2>
          </div>
          ${renderLinkBadge(port)}
        </div>
        <div class="inspector-body">
          ${this.renderPortMembership(edit, port)}
          <p>Current PVID: ${currentPvid ? networkName(currentPvid.original) : "Not reported"}</p>
        </div>
      </section>
    `;
  }

  private renderPortMembership(edit: VlanEdit, port: Port) {
    const state = edit.draft.ports[port.portNum];
    const wasPvid = Boolean(hex(edit.original.pvid) & (1 << port.logPort));
    return html`
      <div
        class="port-membership-settings entity-color"
        style=${entityColor(edit.draft.id)}
        role="region"
        aria-label=${`Port ${port.portNum} VLAN membership`}
      >
        <fieldset class="membership-options">
          <legend>Outgoing traffic</legend>
          ${membershipOptions.map(
            (option) => html`
              <label class="membership-option">
                <input
                  type="radio"
                  name="membership"
                  value=${option.value}
                  data-port=${port.portNum}
                  .checked=${state.membership === option.value}
                  ?disabled=${option.value === "none" && state.pvid}
                  @change=${this.changeMembership}
                />
                <span>
                  <strong>${option.label}</strong>
                </span>
              </label>
            `,
          )}
        </fieldset>
        <div class="pvid-editor">
          <label class="check">
            <input
              type="checkbox"
              data-port=${port.portNum}
              .checked=${state.pvid}
              ?disabled=${wasPvid || state.membership === "none"}
              @change=${this.changePvid}
            />
            Use this VLAN as PVID
          </label>
          ${
            wasPvid
              ? html`
                  <p>Current PVID; this port must stay a member.</p>
                `
              : nothing
          }
        </div>
      </div>
    `;
  }

  private renderChanges(edit: VlanEdit) {
    const changes = vlanChanges(edit.original, edit.draft, this.ctx.ports);
    const changeTitle = changes.length === 1 ? "1 draft change" : `${changes.length} draft changes`;
    const canDelete =
      edit.original.id > 0 && edit.original.id !== this.vlans.mgmt && !hex(edit.original.pvid);
    return html`
      <section
        class="vlan-change-summary"
        aria-labelledby="vlan-changes-title"
      >
        <h2 id="vlan-changes-title">${changes.length ? changeTitle : "No pending changes"}</h2>
        ${
          changes.length
            ? html`
                <ul>
                  ${changes.map(
                    (change) => html`
                      <li>${change}</li>
                    `,
                  )}
                </ul>
              `
            : nothing
        }
        ${
          edit.original.id === this.vlans.mgmt && changes.length
            ? html`
                <p class="management-warning">
                  This is the management VLAN. Removing access can disconnect this session.
                </p>
              `
            : nothing
        }
        <div class="vlan-review-actions">
          <button
            type="submit"
            class="primary"
            ?disabled=${!changes.length || this.reviewing}
          >
            ${this.reviewing ? "Checking current state…" : "Review changes"}
          </button>
          <button
            type="button"
            ?disabled=${!changes.length || this.reviewing}
            @click=${this.discardDraft}
          >
            Discard draft
          </button>
        </div>
        ${
          canDelete
            ? html`
                <button
                  type="button"
                  class="danger"
                  ?disabled=${this.reviewing || Boolean(changes.length)}
                  @click=${this.deleteVlan}
                >
                  Delete VLAN
                </button>
              `
            : nothing
        }
      </section>
    `;
  }

  protected render() {
    if (this.loading_ || this.error_) return this.state();
    const edit = this.currentEdit;
    const port =
      this.ctx.ports.find((item) => item.portNum === this.ctx.selectedPort) ?? this.ctx.ports[0];
    return html`
      ${
        edit
          ? html`
              <form @submit=${this.submitVlan}>
                <section
                  class="switch-workbench feature-workbench vlan-workbench"
                  aria-label="Switch ports"
                >
                  ${renderDeviceFront({
                    ports: this.ctx.ports,
                    selectedPort: port?.portNum ?? 0,
                    selectedPorts: this.bulkMode ? this.selectedPorts : undefined,
                    overlay: "vlan",
                    vlan: previewVlan(edit.original, edit.draft, this.ctx.ports),
                    onSelect: this.selectPort,
                    onCycleMembership: this.bulkMode ? undefined : this.cyclePortMembership,
                  })}
                  <div class="workbench-networks">${this.renderNetworkPicker()}</div>
                  ${this.renderBulkActions(edit)}
                </section>
                <div class="switch-details">
                  <aside class="connection-browser feature-context vlan-settings">
                    ${this.renderIdentity(edit)} ${this.renderChanges(edit)}
                  </aside>
                  ${
                    this.bulkMode
                      ? html`
                          <section class="port-inspector padded">
                            ${this.renderSelectionSummary()}
                          </section>
                        `
                      : nothing
                  }
                  ${!this.bulkMode && port ? this.renderPortEditor(edit, port) : nothing}
                </div>
              </form>
            `
          : html`
              <p class="empty">No VLANs reported. Create a VLAN to start configuring its ports.</p>
            `
      }
    `;
  }
}

customElements.define("rtl-vlans", VlansPage);
