import { Page, html, nothing, heading, chip, portLabel, refreshButton, api } from "../shared";
import { count, hex, speed, download, type Port } from "../api";
import { mibCounters } from "../counters";
import { renderPortWorkbench } from "../components/port-workbench";
import type { PortAnnotation } from "../components/device-front";

interface MacEntry {
  mac: string;
  vlan: string;
  port: number;
  idx: string;
  type: string;
}

interface CounterReading {
  name: string;
  value: string;
}

export class DiagnosticsPage extends Page {
  static properties = {
    ...Page.properties,
    mode: {},
    search: { state: true },
    sortBy: { state: true },
    counters: { state: true },
    counterPort: { state: true },
    showAllAddresses: { state: true },
  };

  mode = "statistics";
  private entries: MacEntry[] = [];
  private search = "";
  private sortBy = "port";
  private counters: CounterReading[] = [];
  private counterPort = 0;
  private showAllAddresses = false;

  async load() {
    if (this.mode !== "l2") return;
    const entries = new Map<number, MacEntry>();
    let index = 0;

    for (let page = 0; page < 1024 && this.isConnected; page++) {
      const batch = await api.json<MacEntry[]>(`/l2.json?idx=${index}`);
      if (!batch.length) break;

      const repeated = batch.some((entry) => entries.has(hex(entry.idx)));
      for (const entry of batch) entries.set(hex(entry.idx), entry);
      if (repeated || entries.size >= 4096) break;
      if (page === 1023) throw new Error("MAC scan did not complete. Refresh to try again.");

      index = (hex(batch[batch.length - 1].idx) + 1) & 4095;
      await new Promise((resolve) => setTimeout(resolve, 150));
    }
    if (this.isConnected) this.entries = [...entries.values()];
  }

  private physicalPort(entry: MacEntry): string {
    if (entry.port === 9) return "CPU";
    const port = this.ctx.ports.find((item) => item.logPort === entry.port);
    return String(port?.portNum ?? entry.port);
  }

  private get visibleEntries(): MacEntry[] {
    const query = this.search.toLowerCase();
    return this.entries
      .filter((entry) => {
        if (!this.showAllAddresses && entry.port !== this.activePort?.logPort) return false;
        const type = entry.type === "s" ? "static" : "learned";
        return `${entry.mac} ${hex(entry.vlan)} ${this.physicalPort(entry)} ${type}`
          .toLowerCase()
          .includes(query);
      })
      .sort((first, second) => {
        if (this.sortBy === "vlan") return hex(first.vlan) - hex(second.vlan);
        if (this.sortBy === "mac") return first.mac.localeCompare(second.mac);
        return this.physicalPort(first).localeCompare(this.physicalPort(second), undefined, {
          numeric: true,
        });
      });
  }

  private handleSearch(event: Event) {
    this.search = (event.currentTarget as HTMLInputElement).value;
  }

  private handleSort(event: Event) {
    this.sortBy = (event.currentTarget as HTMLSelectElement).value;
  }

  private toggleAddressScope() {
    this.showAllAddresses = !this.showAllAddresses;
  }

  private async removeEntry(event: Event) {
    const index = Number((event.currentTarget as HTMLButtonElement).dataset.index);
    const entry = this.entries.find((item) => hex(item.idx) === index);
    if (!entry || entry.port === 9) return;
    if (!confirm(`Remove MAC entry ${entry.mac} from port ${this.physicalPort(entry)}?`)) return;

    await this.run(async () => {
      const response = await api.json<{ result: number }>(`/l2_del.json?idx=${index}`);
      if (response.result !== 1) throw new Error("The switch did not remove this entry.");
      await this.reload();
    });
  }

  private async readCounters(event: Event) {
    this.counterPort = Number((event.currentTarget as HTMLButtonElement).dataset.port);
    await this.refreshCounters();
  }

  private async refreshCounters() {
    const port = this.counterPort;
    this.counters = [];
    await this.run(async () => {
      const values = await api.json<string[]>(`/counters.json?port=${port}`);
      const readings: CounterReading[] = [];

      for (let index = 0; index < mibCounters.length && index / 4 < values.length; index += 4) {
        const value = BigInt(values[index / 4]);
        const label = String(mibCounters[index]);
        if (mibCounters[index + 1] === 8) {
          if (label) readings.push({ name: label, value: value.toLocaleString() });
          continue;
        }
        if (label) readings.push({ name: label, value: (value >> 32n).toLocaleString() });
        const secondLabel = String(mibCounters[index + 2]);
        if (secondLabel)
          readings.push({ name: secondLabel, value: (value & 0xffffffffn).toLocaleString() });
      }
      if (this.counterPort === port) this.counters = readings;
    });
  }

  private exportStatistics() {
    const rows = this.ctx.ports.map((port) =>
      [port.portNum, port.name, speed(port), port.txG, port.txB, port.rxG, port.rxB]
        .map((value) => `"${String(value).replaceAll('"', '""')}"`)
        .join(","),
    );
    download(
      "port-statistics.csv",
      ["Port,Name,Link,TX good,TX errors,RX good,RX errors", ...rows].join("\n"),
    );
  }

  private renderMacEntry(entry: MacEntry) {
    return html`
      <article class="card padded">
        <div class="toolbar">
          <span class="connection-number">${this.physicalPort(entry)}</span>
          ${chip(entry.type === "s" ? "Static" : "Learned")}
        </div>
        <h2 class="mono">${entry.mac}</h2>
        <p>VLAN ${hex(entry.vlan)} · Port ${this.physicalPort(entry)}</p>
        <div class="form-actions">
          ${
            entry.port !== 9
              ? html`
                  <button
                    data-index=${hex(entry.idx)}
                    @click=${this.removeEntry}
                  >
                    Remove entry
                  </button>
                `
              : nothing
          }
        </div>
      </article>
    `;
  }

  private renderMacEntries() {
    const entries = this.visibleEntries;
    return html`
      ${heading(this.showAllAddresses ? "All learned addresses" : "Addresses on this port", "Search by MAC, VLAN or address type.")}
      <div class="toolbar">
        <label class="search">
          <input
            aria-label="Search connected devices"
            placeholder="Search MAC, port, VLAN or type…"
            @input=${this.handleSearch}
          />
        </label>
        <label class="field">
          <span>Sort by</span>
          <select @change=${this.handleSort}>
            <option value="port">Port</option>
            <option value="mac">MAC address</option>
            <option value="vlan">VLAN</option>
          </select>
        </label>
      </div>
      <p class="muted">${entries.length} matching devices</p>
      <div class="device-cards">${entries.map((entry) => this.renderMacEntry(entry))}</div>
      ${
        entries.length
          ? nothing
          : html`
              <p class="empty">No matching devices.</p>
            `
      }
    `;
  }

  private renderPortStatistics(port: Port) {
    return html`
      <article class="card padded">
        <div class="toolbar">
          <h2>${port.name || portLabel(port)}</h2>
          ${chip(speed(port), Boolean(port.enabled && port.link))}
        </div>
        <p>${portLabel(port)}</p>
        <dl class="reading-grid">
          <div>
            <dt>↓ Received</dt>
            <dd>${count(port.rxG)}</dd>
          </div>
          <div>
            <dt>↑ Transmitted</dt>
            <dd>${count(port.txG)}</dd>
          </div>
          <div>
            <dt>Receive errors</dt>
            <dd>${count(port.rxB)}</dd>
          </div>
          <div>
            <dt>Transmit errors</dt>
            <dd>${count(port.txB)}</dd>
          </div>
        </dl>
        <button
          data-port=${port.portNum}
          @click=${this.readCounters}
        >
          Hardware counters ↗
        </button>
      </article>
    `;
  }

  private renderCounters() {
    if (!this.counterPort || this.counterPort !== this.activePort?.portNum) return nothing;
    return html`
      <section class="card padded">
        <div class="toolbar">
          ${heading(`Port ${this.counterPort} · hardware counters`, "Full MIB counter snapshot.")}
          <button @click=${this.refreshCounters}>Refresh counters</button>
        </div>
        <dl class="reading-grid">
          ${this.counters.map(
            (counter) => html`
              <div>
                <dt>${counter.name}</dt>
                <dd>${counter.value}</dd>
              </div>
            `,
          )}
        </dl>
      </section>
    `;
  }

  protected render() {
    if (this.loading_ || this.error_) return this.state();
    const selected = this.activePort;
    return renderPortWorkbench({
      ctx: this.ctx,
      title: this.mode === "l2" ? "Learned addresses" : "Statistics",
      selected,
      onSelect: this.selectPhysicalPort,
      annotations: new Map(this.ctx.ports.map((port) => [port.portNum, this.portAnnotation(port)])),
      sidebar: this.renderContext(),
      inspector: html`
        ${
          this.mode === "l2"
            ? this.renderMacEntries()
            : html`
                ${selected ? this.renderPortStatistics(selected) : nothing} ${this.renderCounters()}
              `
        }
      `,
    });
  }

  private portAnnotation(port: Port): PortAnnotation {
    if (this.mode === "l2") {
      const total = this.entries.filter((entry) => entry.port === port.logPort).length;
      return { label: `${total} addresses`, tone: total ? "active" : "muted" };
    }
    try {
      const errors = BigInt(port.rxB) + BigInt(port.txB);
      if (errors) return { label: `${errors.toLocaleString()} errors`, tone: "warning" };
      return { label: "No errors", detail: `${count(port.rxG)} RX` };
    } catch {
      return { label: "Counters unknown" };
    }
  }

  private renderContext() {
    if (this.mode === "l2")
      return html`
        <h2>Connected devices</h2>
        <div class="port-choice-actions">
          <button
            @click=${this.toggleAddressScope}
            aria-pressed=${this.showAllAddresses}
          >
            ${this.showAllAddresses ? "Show selected port" : "Show all ports & CPU"}
          </button>
          ${refreshButton(this)}
        </div>
      `;
    return html`
      <h2>Traffic & errors</h2>
      <button @click=${this.exportStatistics}>Export all ports as CSV</button>
    `;
  }
}

customElements.define("rtl-diagnostics", DiagnosticsPage);
