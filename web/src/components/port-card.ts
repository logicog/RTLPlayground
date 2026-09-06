import { LitElement, html } from "lit";
import { count, speed, type Port } from "../api";
import { chip, portLabel } from "../shared";

/** Emits selection only; configuration belongs to the owning page. */
export class PortCard extends LitElement {
  static properties = {
    port: { attribute: false },
    selected: { type: Boolean },
  };

  port!: Port;
  selected = false;

  protected createRenderRoot() {
    return this;
  }

  private selectPort() {
    this.dispatchEvent(
      new CustomEvent("port-select", {
        detail: this.port.portNum,
        bubbles: true,
      }),
    );
  }

  protected render() {
    const connected = Boolean(this.port.enabled && this.port.link);
    return html`
      <button
        class="port-card ${this.selected ? "selected" : ""}"
        aria-pressed=${this.selected}
        @click=${this.selectPort}
      >
        <span class="port-card-top">
          <span
            class="port-glyph"
            aria-hidden="true"
          >
            ▥
          </span>
          ${chip(speed(this.port), connected)}
        </span>
        <strong>${this.port.name || portLabel(this.port)}</strong>
        <span class="muted">
          ${portLabel(this.port)} · ${this.port.isSFP ? "SFP+" : "Ethernet"}
        </span>
        <span class="port-card-bottom">
          <span>↓ ${count(this.port.rxG)} packets</span>
          <span aria-hidden="true">↗</span>
        </span>
      </button>
    `;
  }
}

customElements.define("rtl-port-card", PortCard);
