import { Page, html, nothing, input, select, submit, heading, api } from "../shared";
import { download, type VlanList } from "../api";
import type { PropertyValues } from "lit";
import { idleTimeoutOptions, readIdleTimeout, saveIdleTimeout } from "../domain/session";
import {
  ipv4,
  networkCommands,
  redactPasswords,
  validateConsoleCommand,
} from "../domain/configuration";

export class SystemPage extends Page {
  static properties = {
    ...Page.properties,
    consoleOutput: { state: true },
  };

  private savedConfig = "";
  private vlans: VlanList = { mgmt: 0, vlan: [] };
  private consoleOutput = "";
  private editedSettings = new Set<string>();

  protected updated(changed: PropertyValues) {
    super.updated(changed);
    const previous = changed.get("ctx") as typeof this.ctx | undefined;
    if (
      changed.has("ctx") &&
      this.ctx.savedConfiguration !== undefined &&
      this.ctx.savedConfiguration !== previous?.savedConfiguration
    ) {
      this.savedConfig = this.ctx.savedConfiguration;
      this.requestUpdate();
    }
  }

  private changeAutoSave(event: Event) {
    this.ctx.setAutoSave((event.currentTarget as HTMLInputElement).checked);
  }

  private markSettingEdited(event: Event) {
    this.editedSettings.add((event.currentTarget as HTMLFormElement).dataset.setting!);
  }

  private submitSessionSettings(event: Event) {
    const form = this.form(event);
    try {
      saveIdleTimeout(Number(form.get("idle-minutes")));
      this.rememberFormValues(event.currentTarget as HTMLFormElement);
      this.ctx.notify("Automatic sign-out preference saved in this browser.");
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private renderSessionSettings() {
    return html`
      <form
        class="card padded"
        data-guard
        @submit=${this.submitSessionSettings}
      >
        ${heading("Session", "Choose when this panel signs out after inactivity.")}
        ${select("idle-minutes", readIdleTimeout(), "Automatic sign-out", idleTimeoutOptions)}
        <p class="field-help">
          Saved in this browser. Activity in this tab restarts the timer. Drafts stay available
          after you sign in again.
        </p>
        <p class="field-help">
          The panel keeps the device session alive while allowed. Browser sleep, a restart or
          another login can end it earlier.
        </p>
        <div class="form-actions">${submit("Save session preference")}</div>
      </form>
    `;
  }

  async load() {
    this.savedConfig = await api.request("/config");
    this.vlans = await api.json("/vlanlist");
  }

  private settingCommands(setting: string, form: FormData): string[] {
    switch (setting) {
      case "identity": {
        const hostname = this.value(form, "hostname");
        return hostname === this.ctx.info.hostname ? [] : [`hostname ${hostname}`];
      }
      case "network":
        return networkCommands(this.ctx.info, this.vlans.mgmt, form);
      case "syslog": {
        const address = this.value(form, "ip");
        if (!ipv4(address)) throw new Error("Enter a valid syslog IPv4 address.");
        const [previousAddress = "0.0.0.0", previousPort = "514"] =
          this.ctx.info.syslog_server.split(":");
        const previousEnabled = previousAddress === "0.0.0.0" ? "off" : "on";
        if (
          address === previousAddress &&
          this.value(form, "port") === previousPort &&
          form.get("enabled") === previousEnabled
        )
          return [];
        return [
          `syslog ip ${address}`,
          `syslog port ${form.get("port")}`,
          `syslog ${form.get("enabled")}`,
        ];
      }
      case "password":
        if (form.get("password") !== form.get("confirmation"))
          throw new Error("Passwords do not match.");
        return [`passwd ${form.get("password")}`];
      case "console": {
        const command = this.value(form, "command").trim();
        validateConsoleCommand(command);
        return [command];
      }
      default:
        return [];
    }
  }

  private reviewSettings(event: Event) {
    event.preventDefault();
    try {
      const commands: string[] = [];
      const network: string[] = [];
      let consoleIndex = -1;
      for (const element of this.querySelectorAll<HTMLFormElement>("form[data-setting]")) {
        const setting = element.dataset.setting!;
        if (!this.editedSettings.has(setting)) continue;
        const form = new FormData(element);
        if (setting === "password" && !form.get("password") && !form.get("confirmation")) continue;
        if (setting === "console" && !this.value(form, "command").trim()) continue;
        if (!element.reportValidity()) return;
        const changes = this.settingCommands(setting, form);
        if (setting === "network") network.push(...changes);
        else {
          if (setting === "console") consoleIndex = commands.length;
          commands.push(...changes);
        }
      }
      // Apply management connectivity changes last.
      commands.push(...network);
      if (!commands.length) return;
      this.ctx.review(
        network.length ? "System settings · connection may change" : "System settings",
        commands,
        async (responses) => {
          if (consoleIndex >= 0) {
            const output = `> ${redactPasswords(commands[consoleIndex])}\n${responses[consoleIndex]}\n`;
            this.consoleOutput = (this.consoleOutput + output).slice(-16_000);
          }
          for (const field of this.querySelectorAll<HTMLInputElement>(
            'form[data-setting="password"] input, form[data-setting="console"] input',
          ))
            field.value = "";
          this.editedSettings.clear();
          await this.reload();
        },
      );
    } catch (error) {
      this.ctx.notify((error as Error).message, true);
    }
  }

  private exportConfiguration() {
    download("rtlplayground-config.txt", this.savedConfig);
  }

  private async restartSwitch() {
    if (this.ctx.changes.length) {
      this.ctx.notify("Save your pending configuration before restarting.", true);
      return;
    }
    if (!confirm("Restart this switch now? Network traffic will be interrupted.")) return;
    try {
      await api.request("/reset");
      this.ctx.notify("Restart requested. Wait for the switch to reconnect.");
    } catch {
      this.ctx.notify(
        "Connection closed after requesting restart. Wait and check the device; do not repeat the request.",
      );
    }
  }

  private renderIdentity() {
    const info = this.ctx.info;
    return html`
      <form
        class="card padded"
        data-setting="identity"
        data-guard
        @input=${this.markSettingEdited}
        @submit=${this.reviewSettings}
      >
        ${heading("Device identity", "A familiar name for your network.")}
        <div class="form-grid single">
          ${input("hostname", info.hostname, "Hostname", { pattern: "[A-Za-z0-9][A-Za-z0-9_.-]*", maxLength: 23 })}
          <label class="field">
            <span>Model</span>
            <strong>${info.hw_ver}</strong>
          </label>
          <label class="field">
            <span>MAC address</span>
            <strong class="mono">${info.mac_address}</strong>
          </label>
        </div>
      </form>
    `;
  }

  private renderNetwork() {
    const info = this.ctx.info;
    const vlans = this.vlans.vlan.map(
      (vlan) => [vlan.id, `${vlan.id} · ${vlan.name || "Default"}`] as [number, string],
    );
    return html`
      <form
        class="card padded"
        data-setting="network"
        data-guard
        @input=${this.markSettingEdited}
        @submit=${this.reviewSettings}
      >
        ${heading("Management network", "Changes may disconnect this browser.")}
        <div class="form-grid">
          ${input("ip", info.ip_address, "IP address")}
          ${input("netmask", info.ip_netmask, "Subnet mask")}
          ${input("gw", info.ip_gateway, "Gateway")}
          ${select("vlan", this.vlans.mgmt, "Management VLAN", vlans)}
        </div>
      </form>
    `;
  }

  private renderConfiguration() {
    const bytes = new TextEncoder().encode(this.savedConfig).length;
    const changes = redactPasswords(this.ctx.changes.join("\n"));
    return html`
      <section class="card padded">
        ${heading("Startup configuration", "Preserve existing settings and save changes from this session.")}
        <p>
          Applied changes survive a restart only after saving. Changes made in another tab are not
          collected here.
        </p>
        <div class="toolbar">
          <div>
            <strong>${this.ctx.changes.length} session changes awaiting save</strong>
            <p>${bytes} bytes stored · 4095-byte limit</p>
          </div>
          <div class="actions">
            <button @click=${this.exportConfiguration}>Export configuration</button>
            <button
              class="primary"
              ?disabled=${this.ctx.savingConfiguration || !this.ctx.changes.length}
              @click=${this.ctx.saveConfiguration}
            >
              ${this.ctx.savingConfiguration ? "Verifying…" : "Save to flash"}
            </button>
          </div>
        </div>
        <label class="check auto-save-setting">
          <input
            type="checkbox"
            .checked=${this.ctx.autoSave}
            @change=${this.changeAutoSave}
          />
          Save automatically
        </label>
        <p class="field-help">
          Save to startup configuration after Apply. Preference for this browser.
        </p>
        <details>
          <summary>View stored configuration</summary>
          <pre class="code">${redactPasswords(this.savedConfig)}</pre>
        </details>
        ${
          changes
            ? html`
                <details open>
                  <summary>Pending session changes</summary>
                  <pre class="code">${changes}</pre>
                </details>
              `
            : nothing
        }
      </section>
    `;
  }

  private renderSyslog() {
    const [address = "0.0.0.0", port = "514"] = this.ctx.info.syslog_server.split(":");
    const enabled = address !== "0.0.0.0";
    return html`
      <form
        class="card padded"
        data-setting="syslog"
        data-guard
        @input=${this.markSettingEdited}
        @submit=${this.reviewSettings}
      >
        ${heading("Remote logging", "Forward diagnostic events to your syslog receiver.")}
        <div class="form-grid">
          ${input("ip", address, "Server IP")}
          ${input("port", port, "UDP port", { type: "number", min: 1, max: 65535 })}
          ${select("enabled", enabled ? "on" : "off", "Logging", ["off", "on"])}
        </div>
      </form>
    `;
  }

  private renderPassword() {
    const options = { type: "password", pattern: "[!-~]+", maxLength: 16 };
    return html`
      <form
        class="card padded"
        data-setting="password"
        data-guard
        @input=${this.markSettingEdited}
        @submit=${this.reviewSettings}
      >
        ${heading("Access password", "Keep a copy before changing access credentials.")}
        <div class="form-grid single">
          ${input("password", "", "New password", options)}
          ${input("confirmation", "", "Repeat password", options)}
        </div>
      </form>
    `;
  }

  private renderConsole() {
    return html`
      <section class="card padded">
        ${heading("Advanced console", "Run a reviewed command. Responses remain visible in this tab.")}
        <form
          data-setting="console"
          data-guard
          @input=${this.markSettingEdited}
          @submit=${this.reviewSettings}
        >
          <div class="console-input">
            <span aria-hidden="true">›</span>
            <input
              name="command"
              aria-label="Console command"
              placeholder="Enter a command…"
              maxlength="127"
              required
              autocomplete="off"
            />
          </div>
        </form>
        <pre
          class="code console"
          aria-live="polite"
        >
${this.consoleOutput || "Console ready."}</pre>
      </section>
    `;
  }

  protected render() {
    if (this.loading_ || this.error_) return this.state();
    return html`
      <div class="toolbar">
        <button
          type="button"
          class="primary"
          @click=${this.reviewSettings}
        >
          Review changes
        </button>
      </div>
      <div class="grid-two">${this.renderIdentity()}${this.renderNetwork()}</div>
      ${this.renderConfiguration()}
      <div class="grid-two">${this.renderSyslog()}${this.renderPassword()}</div>
      ${this.renderSessionSettings()} ${this.renderConsole()}
      <section class="card padded">
        <h2>Restart device</h2>
        <p>
          Traffic and management access will be interrupted. Save and export your configuration
          first.
        </p>
        <div class="form-actions">
          <button
            class="danger"
            @click=${this.restartSwitch}
          >
            Restart switch…
          </button>
        </div>
      </section>
    `;
  }
}

customElements.define("rtl-system", SystemPage);
