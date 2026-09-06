import { Page, html, nothing, heading, api } from "../shared";
import { validateImage, download, type Info, type VlanList } from "../api";
import { uploadFirmware } from "../domain/firmware-upload";

type Phase = "idle" | "backup" | "upload" | "complete" | "unknown";

const PHASE_LABELS: Record<Phase, string> = {
  idle: "Ready for an image",
  backup: "Exporting configuration…",
  upload: "Uploading firmware…",
  complete: "Upload accepted. The switch is applying the image.",
  unknown: "Update status unknown",
};

export class FirmwarePage extends Page {
  static properties = {
    ...Page.properties,
    selectedFile: { state: true },
    valid: { state: true },
    progress: { state: true },
    phase: { state: true },
    failure: { state: true },
  };

  private selectedFile: File | null = null;
  private valid = false;
  private progress = 0;
  private phase: Phase = "idle";
  private failure = "";

  get hasPendingChanges() {
    return Boolean(this.selectedFile) && this.phase === "idle";
  }

  private async chooseImage(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const file = input.files?.[0] || null;
    this.selectedFile = file;
    this.valid = false;
    this.failure = "";
    if (!file) return;

    try {
      validateImage(new Uint8Array(await file.arrayBuffer()));
      if (this.selectedFile !== file) return;
      const machine = this.ctx.info.hw_ver.split(" ")[0];
      if (!file.name.endsWith(`-${machine}.bin`)) {
        throw new Error(
          `Image filename must end with -${machine}.bin. Verify the exact hardware target.`,
        );
      }
      if (!/^(1|2|4|8|16) MB$/.test(this.ctx.info.flash_size)) {
        throw new Error("The device must report at least 1 MB flash for remote updates.");
      }
      this.valid = true;
    } catch (error) {
      this.failure = (error as Error).message;
    }
  }

  private updateProgress = (percent: number) => {
    this.progress = percent;
  };

  private async backupConfiguration() {
    const config = await api.request("/config");
    if (!config.trim()) throw new Error("Configuration export is empty. Upload stopped.");

    const vlans = await api.json<VlanList>("/vlanlist");
    for (const vlan of vlans.vlan) {
      const pattern = new RegExp(`^vlan\\s+${vlan.id}(?:\\s|$)`, "m");
      if (!pattern.test(config)) {
        throw new Error(
          `Active VLAN ${vlan.id} is missing from startup configuration. Resolve this before uploading.`,
        );
      }
    }
    download("rtlplayground-before-update.txt", config);
  }

  private async submitUpload(event: Event) {
    event.preventDefault();
    if (!this.valid || !this.selectedFile || this.phase !== "idle") return;
    if (this.ctx.changes.length) {
      this.failure = "Save pending configuration before updating firmware.";
      return;
    }

    const confirmed = confirm(
      `Upload ${this.selectedFile.name} and restart? Losing power during firmware copying can make the switch unbootable. Continue only with a verified recovery method.`,
    );
    if (!confirmed) return;
    this.ctx.setBusy(true);
    this.phase = "backup";
    this.failure = "";

    try {
      await this.backupConfiguration();
      this.phase = "upload";
      await uploadFirmware(this.selectedFile, this.updateProgress);
      this.progress = 100;
      this.phase = "complete";
    } catch (error) {
      this.failure = (error as Error).message;
      if (this.phase === "backup" || import.meta.env.DEV) {
        this.phase = "idle";
        this.ctx.setBusy(false);
      } else {
        this.phase = "unknown";
      }
    }
  }

  private async checkConnection() {
    await this.run(async () => {
      const info = await api.json<Info>("/information.json");
      this.ctx.notify(
        `Switch is responding with firmware ${info.sw_ver}. Verify its settings before proceeding.`,
      );
      this.ctx.setBusy(false);
    });
  }

  private renderInformation() {
    const info = this.ctx.info;
    return html`
      <section class="card padded">
        ${heading("Firmware update", "Install a verified RTLPlayground image from your computer.")}
        <dl class="reading-grid">
          <div>
            <dt>Running firmware</dt>
            <dd class="mono">${info.sw_ver}</dd>
          </div>
          <div>
            <dt>Hardware</dt>
            <dd>${info.hw_ver}</dd>
          </div>
          <div>
            <dt>Built</dt>
            <dd>${info.build_date}</dd>
          </div>
          <div>
            <dt>Flash capacity</dt>
            <dd>${info.flash_size}</dd>
          </div>
        </dl>
        <div class="notice">
          Updates overwrite active firmware after staging. Keep power stable. There is no automatic
          rollback.
        </div>
      </section>
    `;
  }

  private renderConfirmation() {
    return html`
      <label class="check">
        <input
          type="checkbox"
          required
        />
        I have backed up settings and verified recovery for this device.
      </label>
      <label class="check">
        <input
          type="checkbox"
          required
        />
        Power will remain connected throughout the update.
      </label>
      <div class="form-actions">
        <button
          class="primary"
          ?disabled=${!this.valid}
        >
          Upload firmware…
        </button>
      </div>
    `;
  }

  private renderProgress() {
    const canCheck = this.phase === "complete" || this.phase === "unknown";
    return html`
      <div role="status">
        <strong>${PHASE_LABELS[this.phase]}</strong>
        <progress
          max="100"
          value=${this.progress}
        ></progress>
        <p>Keep this tab open. Do not disconnect power.</p>
        ${
          canCheck
            ? html`
                <button
                  type="button"
                  @click=${this.checkConnection}
                >
                  Check connection
                </button>
              `
            : nothing
        }
      </div>
    `;
  }

  protected render() {
    return html`
      <div class="grid-two">
        ${this.renderInformation()}
        <form
          class="card padded"
          @submit=${this.submitUpload}
        >
          <h2>Choose an image</h2>
          <label class="upload-area">
            <svg
              viewBox="0 0 24 24"
              width="36"
              fill="none"
              stroke="currentColor"
              stroke-width="1.5"
              aria-hidden="true"
            >
              <path d="M12 16V3m-5 5 5-5 5 5M4 16v5h16v-5" />
            </svg>
            <strong>${this.selectedFile?.name || "Select firmware file"}</strong>
            <span>RTLPlayground .bin · exactly 512 KiB</span>
            <input
              type="file"
              accept=".bin"
              aria-label="Firmware image"
              ?disabled=${this.phase !== "idle"}
              @change=${this.chooseImage}
            />
          </label>
          ${
            this.valid
              ? html`
                  <div class="notice good">
                    Image header, size, CRC and target filename checked.
                  </div>
                `
              : nothing
          }
          ${
            this.failure
              ? html`
                  <div
                    role="alert"
                    class="notice error"
                  >
                    ${this.failure}
                  </div>
                `
              : nothing
          }
          ${this.phase === "idle" ? this.renderConfirmation() : this.renderProgress()}
        </form>
      </div>
    `;
  }
}

customElements.define("rtl-firmware", FirmwarePage);
