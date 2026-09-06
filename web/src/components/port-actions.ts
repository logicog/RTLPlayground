import { html } from "lit";

/** A visible tool explains what the next click on a physical port will do. */
export function renderPortActions(
  selected: string,
  options: readonly (readonly [string, string])[],
  onSelect: (event: Event) => void,
) {
  return html`
    <div class="port-action-bar">
      <div
        class="port-choice-actions"
        role="group"
        aria-label="Port click action"
      >
        ${options.map(
          ([value, label]) => html`
            <button
              type="button"
              data-action=${value}
              aria-pressed=${selected === value}
              @click=${onSelect}
            >
              ${label}
            </button>
          `,
        )}
      </div>
    </div>
  `;
}
