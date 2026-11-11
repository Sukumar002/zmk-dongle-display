# Dongle Display Architecture Revision

## Working branch and immutable reference points

- Working branch: `architecture/revision`
- Initial two-parent integration commit: `29a3e358775de7fcf370e7b410cef98e7b912811`
- Chocofi v1.1.0 hardware-tested display-module pin: `0bb5f84ef441d5a23f19006974f4f80a2bea74fa`
- Chocofi consuming ZMK fork pin: `0e82b8fb198088c9242cad7fb341e83573da65dc`
- The working branch contains a two-parent integration commit preserving both the repository's main lineage and the Chocofi-tested pin. No changes have been made to `main`.

## Goals

1. Preserve the known-good 128x64 OLED experience while refactoring.
2. Build a genuine, independent 128x32 renderer; do not scale or squeeze the 128x64 dashboard.
3. Introduce a shared display model so both renderers consume the same normalized device state.
4. Make physical left/right battery identity explicit and robust across connection order, disconnect, and reconnect.
5. Keep keyboard-specific integrations (Chocofi, Hana, then Sweep/Sofle) thin and isolated.

## Physical-side identity: audited contract

The Chocofi v1.1.0 configuration enables `CONFIG_ZMK_SPLIT_BLE_CENTRAL_SIDE_IDENTITY=y` on the central dongle and exposes side IDs on both peripherals:

- Left peripheral: `CONFIG_ZMK_SPLIT_BLE_PERIPHERAL_SIDE_ID=1`
- Right peripheral: `CONFIG_ZMK_SPLIT_BLE_PERIPHERAL_SIDE_ID=2`

In the pinned ZMK fork, the peripheral exposes a read-only encrypted GATT side-ID characteristic. The central reads it after BLE security is established, accepts only IDs 1 and 2, rejects duplicate IDs, and stores the identity against the connection slot. Battery notifications are ignored until identity is known; after identity validation, the central requests a fresh battery read. Battery event sources are normalized as `side_id - 1` (left=0, right=1). Disconnect battery-clear events use the same physical-side source when the ID is known.

The display module currently offsets the event source for an optional dongle battery and indexes its battery widget/cache from that value. Therefore, with the Chocofi configuration and pinned ZMK fork, the module is consuming a side-normalized source—not raw BLE connection order. Do not add a second side-ID conversion.

### Required hardening

- Document the event-source contract at the display-module adapter boundary.
- Make side identity an explicit model enum: unknown, left, right.
- Validate the source before indexing arrays; unknown/out-of-range sources must never become left or right by default.
- Keep unknown identity visually neutral/hidden until validated.
- Keep duplicate-side and missing-side cases safe.
- Ensure disconnect clears only the matching physical side.
- Verify charge-state events follow the same source semantics before routing them through the model.
- Add tests for left-first, right-first, reconnect in both orders, one side missing, duplicate/invalid identity, and battery updates after identity validation.

## Shared model and renderer boundaries

### Shared model

The model owns normalized state and must not depend on LVGL objects or pixel coordinates. Initial state groups:

- Output: USB readiness, selected/preferred transport, Bluetooth profile number, bonded/connected state.
- Battery: dongle battery (optional), left battery, right battery; each with validity, percentage, and charging state.
- Keyboard: active layer identity/name and layout-shift mode.
- Feedback: display-timeout and keyboard-idle-timeout notifications, including hide/hold/timed feedback.
- Base dashboard mode: Cat, text, or WPM where enabled by the consuming keyboard.

Event adapters translate ZMK events into model updates. Renderers receive model snapshots/updates and own all LVGL objects, geometry, fonts, and visibility decisions.

### Independent renderers

- 128x64 renderer: preserve the validated full dashboard and its current visual geometry while moving data ownership into the shared model.
- 128x32 renderer: purpose-built compact layout. Do not instantiate the 128x64 layout and merely disable selected widgets.
- Renderer selection must be based on the actual display resolution, with a safe failure path for unsupported dimensions.

### Display behavior requirements

128x64:
- Connection indicators at top-left.
- Battery percentages at top-right, with physical left/right ordering.
- Layer name at bottom-right.
- Layout-shift indicator at bottom-left.
- Timeout feedback as a full-screen overlay.
- USB plug-in keyboard-name screen takes priority over the dashboard.

128x32:
- Full-size USB/Bluetooth indicators at top-left.
- Compact, vertically stacked left/right battery percentages at top-right.
- On non-base layers, the layer name temporarily replaces the battery area.
- Base layer always shows battery information.
- Compact WIN/MAC layout-shift indicator.
- Timeout feedback as a full-screen overlay.
- USB plug-in keyboard-name screen takes priority over the dashboard.

Timeout and plug-in screens are exclusive presentation states: they temporarily hide the dashboard and restore it cleanly afterwards.

## Implementation sequence

1. **Baseline and dependency audit** — preserve the tested Chocofi pin and current Hana main lineage; trace ZMK and module APIs before changing them.
2. **Battery identity contract** — harden and test the side-normalized event adapter without changing the 128x64 appearance.
3. **Shared model** — move normalized state out of widget-specific rendering code.
4. **128x64 renderer migration** — port the existing working layout with no intentional visual changes.
5. **128x32 renderer** — implement the compact layout independently and test geometry/state transitions.
6. **Integration** — validate Chocofi first, Hana second; Sweep/Sofle only after the shared interface is stable.
7. **CI and release** — build all supported combinations, document pins and migration, and release only after hardware validation.

## Safety and release policy

- Work only on `architecture/revision` until a reviewed PR is ready.
- Do not modify the display repository's `main` during implementation.
- Do not change keyboard configuration repositories as part of the module refactor.
- Do not remove or rewrite existing settings keys without a migration plan.
- Keep the 128x64 implementation as the regression baseline.
- CI success is not hardware validation.
- No release is created without explicit authorization.


## Additional source audit findings

### Resolution handling

The current `custom_status_screen.c` creates one shared layout regardless of display height. The 128x32 configuration disables selected widgets, but there is no independent 128x32 renderer or layout-selection boundary. The timeout widget itself already uses the active display resolution for its full-screen overlay; that behavior should be retained and shared.

The 128x32 Chocofi config currently enables the optional dongle-battery field as well as the two peripheral battery fields. The compact target layout calls for the two physical halves only; the new 128x32 renderer should not inherit the 128x64 battery widget geometry or its extra central-battery slot.

### Charging telemetry transport

The optional Hana charging adapter is board-specific: it samples GPIO0 pin 17 (XIAO nRF52840 charging status) and emits `ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_CHARGE_STATUS_EVENT`. In the pinned ZMK BLE service, `zmk_split_transport_peripheral_bt_report_event()` handles key, sensor, input, and standard battery events, but does not handle `CHARGE_STATUS_EVENT`; it falls through to `-ENOTSUP`. The central event handler has a charge-status case, but the BLE peripheral service currently does not transport this custom event.

Do not treat peripheral charging telemetry as working over BLE until the transport is implemented and tested. Keep the XIAO GPIO adapter outside the generic display model; if retained, make it an optional Hana integration.

### Hana-specific coupling

- `src/hana_base_view.c` and `include/hana_base_view.h` own Cat/Text/WPM base-view state and settings.
- The shared shield CMake currently compiles `src/hana_base_view.c` directly.
- `src/hana_charge_status.c` is optional but tied to XIAO GPIO0.17.

The refactor should expose generic display-model/base-view interfaces and keep Hana-specific policy/hardware adapters behind an optional integration boundary. Preserve existing Hana settings keys or provide an explicit migration path; do not silently reset user preferences.


### CI dependency alignment

The module's sample `config/west.yml` originally tracked upstream ZMK `main`, which lacks fork-specific headers used by the module. The architecture branch now pins the same ZMK fork commit as Chocofi v1.1.0 (`0e82b8fb198088c9242cad7fb341e83573da65dc`). This is required for the module's own CI to compile the current runtime-timeout, side-identity, and charging event APIs. The repository `main` manifest remains unchanged.


### Existing widget/model coupling to unwind

- `battery_status.c` currently combines ZMK event decoding, source normalization, cached battery state, charging animation, dongle-battery persistence, LVGL canvas drawing, and fixed pixel coordinates. These responsibilities should be split so the model owns normalized battery state and the renderer owns drawing/layout.
- `custom_status_screen.c` is currently the single screen constructor and contains all widget placement. It should become a small resolution dispatcher; each renderer should own its own composition and geometry.
- `timeout_status.c` already provides a reusable full-screen timeout surface and keyboard-name banner. Preserve its exclusive overlay/restore behavior while making its sizing resolution-aware through the renderer/model interface.
- `hana_base_view.c` currently controls widgets directly. Generalize the view-mode state/API while retaining the existing Hana settings namespace and migration behavior for compatibility.
