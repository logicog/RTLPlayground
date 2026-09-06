import { LitElement, html, nothing, type PropertyValues } from "lit";
import { api, type Port, type Info } from "./api";
export { html, nothing };
export interface Context {
  ports: Port[];
  info: Info;
  changes: string[];
  autoSave: boolean;
  savingConfiguration: boolean;
  savedConfiguration?: string;
  setAutoSave: (enabled: boolean) => void;
  saveConfiguration: () => Promise<void>;
  selectedPort: number;
  selectPort: (port: number) => void;
  review: (
    title: string,
    commands: string[],
    after?: (responses: string[]) => Promise<void>,
  ) => void;
  notify: (message: string, error?: boolean) => void;
  changed: () => void;
  setBusy: (busy: boolean) => void;
}
export class Page extends LitElement {
  static properties = {
    ctx: { attribute: false },
    loading_: { state: true },
    error_: { state: true },
    revision_: { state: true },
  };
  ctx!: Context;
  loading_ = true;
  error_ = "";
  revision_ = 0;
  private formBaselines = new Map<HTMLFormElement, string>();
  private lastDraftState?: boolean;

  connectedCallback() {
    super.connectedCallback();
    this.setAttribute("data-draft-page", "");
    this.addEventListener("input", this.refreshDraftState);
    this.addEventListener("change", this.refreshDraftState);
  }

  disconnectedCallback() {
    this.removeEventListener("input", this.refreshDraftState);
    this.removeEventListener("change", this.refreshDraftState);
    super.disconnectedCallback();
  }

  private refreshDraftState = () => this.requestUpdate();

  protected updated(_changed: PropertyValues) {
    for (const form of this.formBaselines.keys()) {
      if (!this.contains(form)) this.formBaselines.delete(form);
    }
    for (const form of this.querySelectorAll<HTMLFormElement>("form[data-guard]")) {
      if (!this.formBaselines.has(form)) this.rememberFormValues(form);
    }
    const dirty = this.hasPendingChanges;
    if (dirty !== this.lastDraftState) {
      this.lastDraftState = dirty;
      this.dispatchEvent(new Event("draft-state-changed", { bubbles: true }));
    }
  }

  private formValues(form: HTMLFormElement) {
    return JSON.stringify(
      [...new FormData(form)].map(([name, value]) => [
        name,
        typeof value === "string" ? value : [value.name, value.size, value.lastModified],
      ]),
    );
  }

  protected rememberFormValues(form: HTMLFormElement) {
    this.formBaselines.set(form, this.formValues(form));
  }

  protected get hasFormChanges() {
    return [...this.formBaselines].some(
      ([form, before]) => this.contains(form) && this.formValues(form) !== before,
    );
  }

  get hasPendingChanges(): boolean {
    return this.hasFormChanges;
  }

  async refreshWithConfirmation() {
    if (this.hasFormChanges && !confirm("Discard unapplied changes and refresh?")) return;
    await this.reload();
  }
  protected get activePort(): Port | undefined {
    return (
      this.ctx.ports.find((port) => port.portNum === this.ctx.selectedPort) ?? this.ctx.ports[0]
    );
  }
  protected selectPhysicalPort = (event: Event) => {
    this.ctx.selectPort(Number((event.currentTarget as HTMLButtonElement).dataset.port));
  };
  protected createRenderRoot() {
    return this;
  }
  protected firstUpdated(_changed: PropertyValues) {
    void this.reload();
  }
  async load(): Promise<void> {}
  async reload() {
    this.loading_ = true;
    this.error_ = "";
    try {
      await this.load();
      this.revision_++;
    } catch (error) {
      this.error_ = String((error as Error).message);
    } finally {
      this.loading_ = false;
    }
  }
  state() {
    return this.loading_
      ? html`
          <div
            class="empty"
            role="status"
          >
            <span class="spinner"></span>
            Reading switch state…
          </div>
        `
      : this.error_
        ? html`
            <div
              class="notice error"
              role="alert"
            >
              ${this.error_}
              <button @click=${() => this.reload()}>Try again</button>
            </div>
          `
        : nothing;
  }
  form(event: Event): FormData {
    event.preventDefault();
    return new FormData(event.currentTarget as HTMLFormElement);
  }
  value(form: FormData, key: string) {
    return String(form.get(key) ?? "");
  }
  review(title: string, commands: string[]) {
    if (commands.length) this.ctx.review(title, commands, () => this.reload());
  }
  async run(work: () => Promise<void>) {
    try {
      await work();
    } catch (e) {
      this.ctx.notify((e as Error).message, true);
    }
  }
}
export const input = (
  name: string,
  value: string | number,
  label: string,
  options: {
    type?: string;
    min?: number;
    max?: number;
    pattern?: string;
    maxLength?: number;
    step?: number;
  } = {},
) => html`
  <label class="field">
    <span>${label}</span>
    <input
      name=${name}
      .value=${String(value)}
      type=${options.type || "text"}
      min=${options.min ?? nothing}
      max=${options.max ?? nothing}
      step=${options.step ?? nothing}
      pattern=${options.pattern ?? nothing}
      maxlength=${options.maxLength ?? nothing}
      required
    />
  </label>
`;
export const select = (
  name: string,
  value: string | number,
  label: string,
  options: (string | [string | number, string])[],
) => html`
  <label class="field">
    <span>${label}</span>
    <select name=${name}>
      ${options.map((o) => {
        const [v, l] = Array.isArray(o) ? o : [o, o];
        return html`
          <option
            value=${String(v)}
            ?selected=${String(v) === String(value)}
          >
            ${l}
          </option>
        `;
      })}
    </select>
  </label>
`;
export const check = (name: string, value: boolean, label: string) => html`
  <label class="check">
    <input
      type="checkbox"
      name=${name}
      .checked=${value}
    />
    ${label}
  </label>
`;
export const submit = (label = "Review changes") => html`
  <button
    class="primary"
    type="submit"
  >
    ${label}
    <span aria-hidden="true">↗</span>
  </button>
`;
export const heading = (title: string, description: string) => html`
  <div class="section-heading">
    <div>
      <h2>${title}</h2>
      <p>${description}</p>
    </div>
  </div>
`;
export const chip = (text: string, good = false) => html`
  <span class="chip ${good ? "good" : ""}">
    <i></i>
    ${text}
  </span>
`;
export const portLabel = (p: Port) => (p.isSFP ? `SFP+ ${p.portNum}` : `Port ${p.portNum}`);
export const maskPorts = (mask: number, ports: Port[]) =>
  ports
    .filter((p) => mask & (1 << p.logPort))
    .map((p) => p.portNum)
    .join(", ") || "—";
export const refreshButton = (page: Page) => html`
  <button
    ?disabled=${page.loading_}
    @click=${() => page.refreshWithConfirmation()}
  >
    Refresh
  </button>
`;
export { api };
