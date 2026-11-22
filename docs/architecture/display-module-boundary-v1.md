# Display Module Boundary — v1

Status: implementation contract draft  
Working branch: architecture/display-modular-v1  
Reference firmware: 0e82b8fb198088c9242cad7fb341e83573da65dc

## Purpose

This repository owns reusable dongle status presentation. It does not own battery measurement, charging detection, split identity discovery, BLE connection management, MCU pin configuration, or keyboard-specific layout policy.

The display module consumes normalized state supplied by firmware APIs and renders it through the Zephyr/ZMK display subsystem. It may depend on LVGL and the ZMK display API, but it must not depend on a particular OLED controller or physical bus.

## Ownership

### In scope
- Status model and event-to-model adapter.
- Reusable status widgets.
- Resolution-specific layout/renderers.
- Display timeout and temporary feedback presentation.
- Display-specific Kconfig options and devicetree bindings.
- Build samples for no-display, 128x32 and 128x64 configurations.

### Out of scope
- GPIO polling or charging pin configuration.
- Battery sensor selection or voltage-to-percentage conversion.
- USB/VBUS detection.
- BLE scanning, GATT discovery, peripheral connection lifecycle.
- Physical side identity discovery or assignment.
- MCU/board selection and pin mapping.
- Keyboard key positions, matrix dimensions, layer names or board-specific keymap policy.

## Normalized model contract

The display consumes a snapshot with these independent domains:

### Transport
- USB: unavailable, disconnected, connected, error.
- Bluetooth: unavailable, disconnected, connected, error.
- Active output/profile: optional value supplied by the firmware endpoint API.

### Peripheral identity
- Source: central, left peripheral, right peripheral, unknown peripheral.
- Connection: disconnected, connecting, connected, error.
- Identity validity: unknown or validated.

The renderer must never infer left/right from connection order. Unknown identity remains unknown and is not rendered as a physical side.

### Battery
Each source has:
- measurement availability: unsupported, unknown, valid, error;
- percentage: 0–100 only when valid;
- battery presence: unsupported, unknown, present, absent;
- charging: unsupported, unknown, charging, not charging.

A disconnected peripheral transitions to unknown/unavailable. It must not display 0%. Charging unsupported is not the same as not charging.

### Keyboard status
Layer name/index, WPM, modifier indicators, lock indicators and optional animation state are inputs from generic ZMK APIs. The display module does not define board keymaps or decide which layers count as base layers. Board-level configuration may provide labels or a base-layer policy through a declared interface.

### Temporary feedback
Timeout/alert/name banners are presentation events with explicit duration and priority. They temporarily replace the dashboard and must restore the latest model snapshot after dismissal; they must not restore a stale cached screen.

## Renderer contract

- Renderer selection is based on declared panel dimensions/capabilities, not keyboard identity.
- 128x32 and 128x64 are separate layouts with explicit geometry.
- Unsupported dimensions fail with a useful diagnostic or select a documented safe fallback.
- No-display builds exclude renderer objects and must still compile all non-display modules.
- Temporary overlays use the active panel's actual dimensions.
- Renderers own LVGL objects and geometry. The model owns no LVGL pointers, canvas buffers or pixel coordinates.

## Hardware boundary

The panel/controller and bus are configured by Zephyr devicetree and the selected MCU/board hardware adapter. This module consumes the display device made available by the ZMK display subsystem. It does not configure I2C pins, reset GPIOs, panel offsets or controller-specific commands.

## Battery and identity integration

The selected firmware revision already exposes battery state events and split battery transport. The selected baseline also adds encrypted physical side identity to split peripherals and maps validated left/right identity to normalized source IDs before battery events are delivered.

The display module must consume that normalized source contract once. It must not repeat side-ID discovery, re-map connection slots, or create a second identity protocol.

The current display implementation contains an optional charging source that directly reads GPIO0 pin 17 with active-low semantics for one XIAO design. This is hardware-specific and must be removed from this repository. The charging provider belongs in a separately selected hardware capability adapter; the generic display module only consumes the normalized charging state.

The current battery widget also includes a nice!nano V2 VDDH/USB inference workaround. That logic is not a generic renderer responsibility. Battery presence and validity must be supplied by a hardware/source adapter or by the firmware battery API contract. The widget should render those states, not infer them from MCU-specific voltage behaviour.

## Build and test targets

Minimum CI matrix:
- no display;
- 128x32 panel;
- 128x64 panel;
- display module enabled with dongle battery omitted;
- display module enabled with peripheral batteries only;
- display module enabled with central and peripheral battery sources;
- charging capability unsupported;
- charging capability present and valid.

Static checks:
- no board/shield names in generic source or Kconfig;
- no GPIO pin numbers or polarity in generic source;
- no MCU-family or board-target conditionals in generic source;
- no battery measurement or BLE identity discovery code in widgets;
- every Kconfig option has a clear owner and dependency;
- every C source is included only under its owning Kconfig guard.

Runtime/model tests:
- left-first and right-first connection;
- reconnect in reverse order;
- one peripheral missing;
- unknown/invalid/duplicate identity;
- battery update before identity validation;
- disconnect clears only the identified side;
- unsupported charging remains visually distinct from not charging;
- battery unavailable is not rendered as 0%;
- alert overlay dismisses to the newest dashboard state.

## Migration plan

1. Keep the existing 128x64 renderer as the visual reference.
2. Extract the normalized model and event adapter from LVGL widget files.
3. Make 128x64 render from the model without intentional visual changes.
4. Implement 128x32 as an independent layout.
5. Move hardware charging detection to a hardware capability adapter.
6. Remove MCU-specific battery-presence inference from the generic widget.
7. Add no-display and cross-MCU build coverage.
8. Port board configurations only after the model and renderer contracts are stable.

## Acceptance

The module is portable when the same immutable module revision builds with XIAO nRF52840 and Pro Micro/nice!nano-compatible targets, provided each target supplies the required display and battery capabilities, and when the same module revision serves different board shields without keyboard-specific source changes.

A build that passes compilation but relies on undocumented event-source ordering, hidden GPIO assumptions or stale battery state does not pass.


## Known transitional implementation debt

The current branch is not yet a fully board/MCU-neutral display release. These items are explicitly tracked and must be resolved before promotion:

1. The firmware-side identity overlay still renders the fixed labels HANA and CHOCOFI and owns LVGL objects. It must be converted to a generic identity event/model with labels supplied by the consuming board configuration, and the renderer must live with this display module.
2. The battery widget still contains a nice!nano-specific battery-presence inference. That inference must move to a battery-source capability adapter; the widget must consume explicit presence/validity state.
3. The optional charging source still polls a XIAO-specific GPIO. It must move to an independently selectable XIAO hardware adapter, not remain part of the display module.
4. The historical settings namespace hana_base_view is retained solely to preserve existing saved preferences. It is a compatibility key, not an active board dependency; a later settings migration may replace it.
5. The current build fixtures are compile-only. Their mock matrix and panel nodes prove MCU/renderer compile compatibility, not physical I2C pin wiring or OLED runtime behaviour.

The first integration milestone may preserve these existing adapters behind explicit configuration so current board builds remain functional. It must not be described as fully board/MCU independent until items 1–3 have been separated and the acceptance matrix passes.

6. The firmware declares a split CHARGE_STATUS_EVENT, but the inspected BLE split service currently handles key/input/sensor/battery events without forwarding this charge-status event to the central. Peripheral charging icons therefore cannot be claimed as functional yet. Add a side-aware charge-status characteristic/notification (or equivalent transport) and validate disconnect/reconnect state before enabling the UI path.


## Current validation status

The isolated module build workflow passed all six compile targets: no-display, 128x32 and 128x64 on both xiao_ble and nice_nano board targets. The 128x32 targets compile with the compact layout option; 128x64 targets compile with the full dashboard. The test shields use each board's declared I2C alias and a mock kscan matrix.

This validates compilation and optional-module composition only. It does not validate panel wiring, OLED readability, charging hardware, battery accuracy or BLE runtime transitions.
