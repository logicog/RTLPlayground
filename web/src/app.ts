import { LitElement, html, nothing } from "lit";
import { keyed } from "lit/directives/keyed.js";
import { api, type Info, type Port, type VlanList } from "./api";
import { type Context, type Page } from "./shared";
import { StartupConfiguration, autoSaveKey, readAutoSave } from "./domain/startup-configuration";
import { navigation } from "./layout/navigation";
import { renderNavigationHeader, renderTopbar, type ShellState } from "./layout/shell";
import {
  idleTimeoutKey,
  idleTimeoutReached,
  readIdleTimeout,
  sessionPreferenceEvent,
} from "./domain/session";
import "./pages/overview";
import "./style.css";
import "./components/cards.css";
import "./layout/shell.css";
if (import.meta.env.DEV) {
  const { demoFetch } = await import("./demo");
  api.useTransport(demoFetch);
}
const configCommand = (cmd: string) =>
  /^(ip|netmask|gw|hostname|passwd|syslog|vlan|pvid|ingress|port|eee|mirror|lag|laghash|isolate|stp|igmp|mtu|bw)\b/.test(
    cmd,
  ) && !/(?:^|\s)show(?:\s|$)/.test(cmd);
export class App extends LitElement {
  static properties = {
    route_: { state: true },
    ready_: { state: true },
    info_: { state: true },
    ports_: { state: true },
    vlans_: { state: true },
    error_: { state: true },
    notice_: { state: true },
    noticeError_: { state: true },
    online_: { state: true },
    busy_: { state: true },
    review_: { state: true },
    expired_: { state: true },
    theme_: { state: true },
    last_: { state: true },
    changes_: { state: true },
    paused_: { state: true },
    selectedPort_: { state: true },
    signingIn_: { state: true },
    signInError_: { state: true },
    idleExpired_: { state: true },
    autoSave_: { state: true },
    savingConfiguration_: { state: true },
    saveError_: { state: true },
    savedConfiguration_: { state: true },
  };
  route_ = "overview";
  ready_ = false;
  info_!: Info;
  ports_: Port[] = [];
  vlans_: VlanList = { mgmt: 0, vlan: [] };
  error_ = "";
  notice_ = "";
  noticeError_ = false;
  online_ = false;
  busy_ = false;
  expired_ = false;
  last_ = 0;
  changes_: string[] = [];
  paused_ = false;
  selectedPort_ = 0;
  signingIn_ = false;
  signInError_ = "";
  idleExpired_ = false;
  private autoSave_ = readAutoSave();
  private savingConfiguration_ = false;
  private saveError_ = "";
  private savedConfiguration_?: string;
  private startup = new StartupConfiguration(api);
  private idleMinutes_ = readIdleTimeout();
  private lastActivity_ = Date.now();
  review_: {
    title: string;
    commands: string[];
    after?: (responses: string[]) => Promise<void>;
  } | null = null;
  theme_ = "light";
  timer_: ReturnType<typeof setTimeout> | undefined;
  lastSession_ = 0;
  generation_ = 0;
  private activeHash_ = "";

  private get hasPendingDrafts() {
    return [...this.querySelectorAll<Page>("[data-draft-page]")].some(
      (page) => page.hasPendingChanges,
    );
  }
  protected createRenderRoot() {
    return this;
  }
  connectedCallback() {
    super.connectedCallback();
    try {
      this.theme_ = localStorage.getItem("rtl-theme") || "light";
    } catch {}
    document.documentElement.dataset.theme = this.theme_;
    window.addEventListener("hashchange", this.navigate_);
    window.addEventListener("session-expired", this.expire_);
    window.addEventListener("beforeunload", this.unload_);
    document.addEventListener("visibilitychange", this.visibility_);
    for (const event of ["pointerdown", "keydown", "wheel"]) {
      document.addEventListener(event, this.recordActivity_, { passive: true });
    }
    window.addEventListener(sessionPreferenceEvent, this.sessionPreferenceChanged_);
    window.addEventListener("storage", this.sessionStorageChanged_);
    this.addEventListener("draft-state-changed", this.draftStateChanged_);
    void this.start();
  }
  disconnectedCallback() {
    super.disconnectedCallback();
    clearTimeout(this.timer_);
    window.removeEventListener("hashchange", this.navigate_);
    window.removeEventListener("session-expired", this.expire_);
    window.removeEventListener("beforeunload", this.unload_);
    document.removeEventListener("visibilitychange", this.visibility_);
    for (const event of ["pointerdown", "keydown", "wheel"])
      document.removeEventListener(event, this.recordActivity_);
    window.removeEventListener(sessionPreferenceEvent, this.sessionPreferenceChanged_);
    window.removeEventListener("storage", this.sessionStorageChanged_);
    this.removeEventListener("draft-state-changed", this.draftStateChanged_);
  }
  private draftStateChanged_ = () => this.requestUpdate();
  unload_ = (e: BeforeUnloadEvent) => {
    if (this.changes_.length || this.busy_ || this.hasPendingDrafts) {
      e.preventDefault();
      e.returnValue = "";
    }
  };
  expire_ = () => {
    this.expired_ = true;
    this.signInError_ = "";
    void this.updateComplete.then(() =>
      this.querySelector<HTMLInputElement>(".session input")?.focus(),
    );
  };
  private sessionPreferenceChanged_ = () => {
    this.idleMinutes_ = readIdleTimeout();
    this.lastActivity_ = Date.now();
  };
  private sessionStorageChanged_ = (event: StorageEvent) => {
    if (event.key === idleTimeoutKey) this.sessionPreferenceChanged_();
    if (event.key === autoSaveKey) this.autoSave_ = readAutoSave();
  };
  private checkIdleTimeout() {
    if (this.expired_ || this.busy_) return;
    if (!idleTimeoutReached(this.lastActivity_, this.idleMinutes_, Date.now())) return;
    document.cookie = "session=; Max-Age=0; Path=/; SameSite=Strict";
    this.idleExpired_ = true;
    this.expire_();
  }
  private recordActivity_ = () => {
    this.checkIdleTimeout();
    if (!this.expired_) this.lastActivity_ = Date.now();
  };
  visibility_ = () => {
    if (!document.hidden && !this.expired_) void this.poll();
  };
  navigate_ = () => {
    void this.navigate();
  };
  notify = (message: string, error = false) => {
    this.notice_ = message;
    this.noticeError_ = error;
  };
  async start() {
    try {
      await this.refresh();
      await this.navigate();
    } catch (e) {
      this.error_ = (e as Error).message;
    }
    this.schedule();
  }
  async refresh() {
    this.info_ = await api.json<Info>("/information.json");
    this.ports_ = (await api.json<Port[]>("/status.json")).sort((a, b) => a.portNum - b.portNum);
    this.vlans_ = await api.json<VlanList>("/vlanlist");
    this.online_ = true;
    this.last_ = Date.now();
    this.error_ = "";
  }
  schedule() {
    clearTimeout(this.timer_);
    if (this.isConnected) this.timer_ = setTimeout(() => this.poll(), 5000);
  }
  async poll() {
    clearTimeout(this.timer_);
    this.checkIdleTimeout();
    if (this.busy_ || this.expired_ || api.pending) {
      this.schedule();
      return;
    }
    try {
      if (Date.now() - this.lastSession_ > 60000) {
        await api.request("/index.html");
        this.lastSession_ = Date.now();
      }
      if (!document.hidden && !this.paused_) {
        this.ports_ = (await api.json<Port[]>("/status.json")).sort(
          (a, b) => a.portNum - b.portNum,
        );
        this.online_ = true;
        this.last_ = Date.now();
      }
    } catch {
      this.online_ = false;
    }
    this.schedule();
  }
  async navigate() {
    if (this.ready_ && location.hash === this.activeHash_) return;
    if (
      this.busy_ ||
      (this.hasPendingDrafts && !confirm("Discard unapplied changes and leave this page?"))
    ) {
      history.replaceState(null, "", `${location.pathname}${location.search}${this.activeHash_}`);
      return;
    }
    this.activeHash_ = location.hash;
    const requested = location.hash.slice(1).split("?")[0] || "overview";
    this.route_ = navigation.some((n) => n[0] === requested) ? requested : "overview";
    this.ready_ = false;
    const generation = ++this.generation_;
    try {
      // The firmware has one HTTP connection. Module loads share the API queue.
      await api.exclusive(async () => {
        if (this.route_ === "ports" || this.route_ === "eee" || this.route_ === "bandwidth")
          await import("./pages/ports");
        else if (this.route_ === "vlans") await import("./pages/vlans");
        else if (["lag", "stp", "mirror"].includes(this.route_)) await import("./pages/switching");
        else if (["statistics", "l2"].includes(this.route_)) await import("./pages/diagnostics");
        else if (this.route_ === "system") await import("./pages/system");
        else if (this.route_ === "firmware") await import("./pages/firmware");
      });
      if (generation !== this.generation_) return;
      this.ready_ = true;
      document.title = `${navigation.find((n) => n[0] === this.route_)?.[1]} · RTLPlayground`;
      await this.updateComplete;
      this.querySelector<HTMLElement>("h1")?.focus();
    } catch (e) {
      this.error_ = "Could not load this page. Check the connection and refresh.";
    }
  }
  ctx(): Context {
    return {
      ports: this.ports_,
      info: this.info_,
      changes: this.changes_,
      autoSave: this.autoSave_,
      savingConfiguration: this.savingConfiguration_,
      savedConfiguration: this.savedConfiguration_,
      setAutoSave: this.setAutoSave,
      saveConfiguration: this.saveConfiguration,
      selectedPort: this.selectedPort_,
      selectPort: (port) => {
        this.selectedPort_ = port;
      },
      review: (title, commands, after) => {
        if (this.busy_) return;
        this.review_ = { title, commands, after };
        void this.updateComplete.then(() =>
          this.querySelector<HTMLDialogElement>("#review")?.showModal(),
        );
      },
      notify: this.notify,
      changed: () => this.requestUpdate(),
      setBusy: (busy) => {
        this.busy_ = busy;
      },
    };
  }
  async apply() {
    const review = this.review_;
    if (!review || this.busy_) return;
    this.busy_ = true;
    let completed = 0;
    const responses: string[] = [];
    try {
      const modifiesConfiguration = review.commands.some(configCommand);
      if (modifiesConfiguration) await this.startup.prepare(this.changes_.length);
      for (const command of review.commands) {
        responses.push(await api.command(command));
        if (configCommand(command)) this.changes_ = [...this.changes_, command];
        completed++;
      }
      this.notify(
        `${completed} change${completed === 1 ? "" : "s"} applied. Save startup configuration to keep them after a restart.`,
      );
      this.review_ = null;
      if (this.autoSave_ && modifiesConfiguration) await this.persistConfiguration();
      await this.refresh();
      await review.after?.(responses);
    } catch (e) {
      this.notify(
        `${completed}/${review.commands.length} commands completed. ${(e as Error).message}`,
        true,
      );
      this.review_ = null;
    } finally {
      this.busy_ = false;
    }
  }

  private setAutoSave = (enabled: boolean) => {
    try {
      localStorage.setItem(autoSaveKey, String(enabled));
      this.autoSave_ = enabled;
    } catch {
      this.notify("Could not save this browser preference.", true);
    }
  };

  private saveConfiguration = async () => {
    if (this.busy_ || !this.changes_.length) return;
    this.busy_ = true;
    try {
      await this.persistConfiguration();
    } finally {
      this.busy_ = false;
    }
  };

  private async persistConfiguration() {
    if (!this.changes_.length) return;
    this.savingConfiguration_ = true;
    this.saveError_ = "";
    try {
      const result = await this.startup.save(this.changes_);
      this.changes_ = this.changes_.slice(result.savedCount);
      this.savedConfiguration_ = result.configuration;
      this.notify("Startup configuration saved and verified.");
    } catch (error) {
      this.saveError_ = (error as Error).message;
      this.notify(`Configuration not verified as saved. ${this.saveError_}`, true);
    } finally {
      this.savingConfiguration_ = false;
    }
  }
  private renderPage() {
    const context = this.ctx();

    switch (this.route_) {
      case "overview":
        return html`
          <rtl-overview
            .ctx=${context}
            .vlans=${this.vlans_}
          ></rtl-overview>
        `;
      case "ports":
      case "eee":
      case "bandwidth":
        return html`
          <rtl-ports
            .ctx=${context}
            .mode=${this.route_}
          ></rtl-ports>
        `;
      case "vlans":
        return html`
          <rtl-vlans .ctx=${context}></rtl-vlans>
        `;
      case "stp":
      case "lag":
      case "mirror":
        return html`
          <rtl-switching
            .ctx=${context}
            .mode=${this.route_}
          ></rtl-switching>
        `;
      case "statistics":
      case "l2":
        return html`
          <rtl-diagnostics
            .ctx=${context}
            .mode=${this.route_}
          ></rtl-diagnostics>
        `;
      case "system":
        return html`
          <rtl-system .ctx=${context}></rtl-system>
        `;
      case "firmware":
        return html`
          <rtl-firmware .ctx=${context}></rtl-firmware>
        `;
    }
  }

  private handleThemeToggle = () => {
    this.theme_ = this.theme_ === "light" ? "dark" : "light";
    document.documentElement.dataset.theme = this.theme_;
    try {
      localStorage.setItem("rtl-theme", this.theme_);
    } catch {
      // The current theme still works when browser storage is unavailable.
    }
  };

  private handlePauseToggle() {
    this.paused_ = !this.paused_;
  }

  private dismissNotice() {
    this.notice_ = "";
  }

  private skipToContent(event: Event) {
    event.preventDefault();
    this.querySelector<HTMLElement>("h1")?.focus();
  }

  private async handleRefresh() {
    try {
      await this.refresh();
      if (this.route_ === "vlans") await this.querySelector<Page>("rtl-vlans")?.reload();
    } catch (error) {
      this.notify((error as Error).message, true);
    }
  }

  private cancelReview(event: Event) {
    if (this.busy_) {
      event.preventDefault();
      return;
    }
    this.review_ = null;
  }

  private async handleSignIn(event: Event) {
    event.preventDefault();
    if (this.signingIn_) return;
    this.signingIn_ = true;
    this.signInError_ = "";
    const element = event.currentTarget as HTMLFormElement;
    const form = new FormData(element);
    const password = String(form.get("password"));

    try {
      await api.signIn(password);
      element.reset();
      this.lastActivity_ = Date.now();
      this.lastSession_ = Date.now();
      this.expired_ = false;
      this.idleExpired_ = false;
      this.notify("Signed in. Your draft is still here.");
      if (!this.info_) await this.start();
    } catch (error) {
      this.signInError_ = (error as Error).message;
    } finally {
      this.signingIn_ = false;
    }
  }

  private renderContent() {
    if (this.error_) {
      return html`
        <div
          class="notice error"
          role="alert"
        >
          ${this.error_}
          <button @click=${this.start}>Try again</button>
        </div>
      `;
    }
    if (!this.info_ || !this.ready_) {
      return html`
        <div
          class="empty"
          role="status"
        >
          <span class="spinner"></span>
          Connecting to your switch…
        </div>
      `;
    }
    return keyed(this.route_, this.renderPage());
  }

  private renderNotice() {
    if (!this.notice_) return nothing;
    return html`
      <div
        class="notice ${this.noticeError_ ? "error" : "good"}"
        role=${this.noticeError_ ? "alert" : "status"}
      >
        <span>${this.notice_}</span>
        <button
          aria-label="Dismiss notification"
          @click=${this.dismissNotice}
        >
          ×
        </button>
      </div>
    `;
  }

  private renderReviewDialog() {
    if (!this.review_) return nothing;
    const commands = this.review_.commands.map((command) =>
      command.startsWith("passwd ") ? "passwd [hidden]" : command,
    );

    return html`
      <dialog
        id="review"
        @cancel=${this.cancelReview}
      >
        <form method="dialog">
          <div class="eyebrow">REVIEW CHANGES</div>
          <h2>${this.review_.title}</h2>
          <p>These changes apply immediately. Network settings may interrupt your connection.</p>
          <details>
            <summary>Commands to apply (${commands.length})</summary>
            <pre class="code">${commands.join("\n")}</pre>
          </details>
          <div class="form-actions">
            <button
              ?disabled=${this.busy_}
              @click=${this.cancelReview}
            >
              Cancel
            </button>
            <button
              type="button"
              class="primary"
              ?disabled=${this.busy_}
              @click=${this.apply}
            >
              ${this.busy_ ? "Applying…" : "Apply changes"}
            </button>
          </div>
        </form>
      </dialog>
    `;
  }

  private renderSignIn() {
    if (!this.expired_) return nothing;
    return html`
      <div
        class="session-backdrop"
        role="dialog"
        aria-modal="true"
        aria-labelledby="session-title"
      >
        <form
          class="card padded session"
          @submit=${this.handleSignIn}
        >
          <h2 id="session-title">Sign in again</h2>
          <p>
            ${this.idleExpired_ ? "Signed out after inactivity." : "Your session expired."} Drafts
            stay in this tab.
          </p>
          <label class="field">
            <span>Password</span>
            <input
              name="password"
              type="password"
              autocomplete="current-password"
              required
            />
          </label>
          ${
            this.signInError_
              ? html`
                  <p
                    class="notice error"
                    role="alert"
                  >
                    ${this.signInError_}
                  </p>
                `
              : nothing
          }
          <div class="form-actions">
            <button
              class="primary"
              ?disabled=${this.signingIn_}
            >
              ${this.signingIn_ ? "Signing in…" : "Sign in"}
            </button>
          </div>
        </form>
      </div>
    `;
  }
  render() {
    const nav = navigation.find((n) => n[0] === this.route_)!;
    const shell: ShellState = {
      route: this.route_,
      info: this.info_,
      online: this.online_,
      onThemeToggle: this.handleThemeToggle,
      draft: this.hasPendingDrafts,
      unsaved: this.changes_.length,
      saving: this.savingConfiguration_,
      saveError: this.saveError_,
      verifySave: this.startup.needsVerification,
      busy: this.busy_ || this.expired_,
      onSave: this.saveConfiguration,
    };

    return html`
      <a
        class="skip"
        href="#main"
        @click=${this.skipToContent}
      >
        Skip to content
      </a>
      ${renderNavigationHeader(shell)}
      <div
        class="workspace-main"
        ?inert=${this.expired_}
      >
        ${renderTopbar(shell)}
        <main id="main">
          ${
            import.meta.env.DEV
              ? html`
                  <p class="preview-label">Development preview · simulated switch data</p>
                `
              : nothing
          }
          <div class="page-heading">
            <div>
              <h1 tabindex="-1">
                ${this.route_ === "overview" ? this.info_?.hostname || "Switch" : nav[1]}
              </h1>
              ${
                this.route_ === "overview"
                  ? html`
                      <p>${this.info_?.ip_address}</p>
                    `
                  : nothing
              }
            </div>
            <div class="actions">
              <button
                class="live-control"
                aria-pressed=${!this.paused_}
                @click=${this.handlePauseToggle}
              >
                <i class="dot ${this.paused_ ? "gray" : ""}"></i>
                ${this.paused_ ? "Live paused" : "Live · 5s"}
              </button>
              <button
                ?disabled=${this.busy_}
                @click=${this.handleRefresh}
              >
                Refresh
              </button>
            </div>
          </div>
          ${this.renderNotice()} ${this.renderContent()}
          <footer>
            <span>
              ${this.info_?.sw_ver || "RTLPlayground"}
              <span class="separator">/</span>
              Local management
            </span>
            <span>
              ${this.last_ ? "Updated " + new Date(this.last_).toLocaleTimeString() : "Connecting"}
            </span>
          </footer>
        </main>
      </div>
      ${this.renderReviewDialog()} ${this.renderSignIn()}
    `;
  }
}
customElements.define("rtl-app", App);
