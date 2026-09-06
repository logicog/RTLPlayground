import { html, nothing } from "lit";

interface BulkActionsState {
  multiple: boolean;
  total: number;
  selected: number;
  reviewing: boolean;
  canExclude: boolean;
  onToggle: (event: Event) => void;
  onSelectAll: () => void;
  onClear: () => void;
  onMembership: (event: Event) => void;
}

export function renderVlanBulkActions(state: BulkActionsState) {
  const scope = state.multiple ? `${state.selected} selected ports` : `All ${state.total} ports`;
  const target = state.multiple ? "selected ports" : "all ports";
  const disabled = state.reviewing || (state.multiple && !state.selected);

  return html`
    <div class="vlan-bulk-toolbar">
      <div class="vlan-selection-actions">
        <label class="check">
          <input
            type="checkbox"
            .checked=${state.multiple}
            @change=${state.onToggle}
          />
          Select multiple ports
        </label>
        ${
          state.multiple
            ? html`
                <button
                  type="button"
                  @click=${state.onSelectAll}
                >
                  Select all
                </button>
                <button
                  type="button"
                  @click=${state.onClear}
                  ?disabled=${!state.selected}
                >
                  Clear selection
                </button>
              `
            : nothing
        }
      </div>
      <div
        class="vlan-membership-actions"
        role="group"
        aria-label="Bulk membership"
      >
        <strong aria-live="polite">${scope}</strong>
        <button
          type="button"
          data-membership="tagged"
          aria-label=${`Set ${target} tagged`}
          ?disabled=${disabled}
          @click=${state.onMembership}
        >
          Tagged
        </button>
        <button
          type="button"
          data-membership="untagged"
          aria-label=${`Set ${target} untagged`}
          ?disabled=${disabled}
          @click=${state.onMembership}
        >
          Untagged
        </button>
        <button
          type="button"
          data-membership="none"
          aria-label=${`Exclude ${target}`}
          ?disabled=${disabled || !state.canExclude}
          @click=${state.onMembership}
        >
          Excluded
        </button>
      </div>
      <p>
        ${state.canExclude ? "Changes affect this VLAN's draft only. PVIDs stay unchanged." : "Ports using this VLAN as PVID cannot be excluded. Tagged / untagged leaves PVIDs unchanged."}
      </p>
    </div>
  `;
}
