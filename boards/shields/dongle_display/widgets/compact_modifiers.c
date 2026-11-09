/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>

#include "modifiers.h"
#include "../compact_renderer.h"

#define HANA_WIN_BASE_LAYER 1
#if DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt))
#define COMPACT_HAS_LAYOUT_SHIFT_MAP 1
static const struct device *const compact_layout_shift_map =
    DEVICE_DT_GET(DT_NODELABEL(layout_shift_map_gui_alt));
#elif DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt_shared))
#define COMPACT_HAS_LAYOUT_SHIFT_MAP 1
static const struct device *const compact_layout_shift_map =
    DEVICE_DT_GET(DT_NODELABEL(layout_shift_map_gui_alt_shared));
#else
#define COMPACT_HAS_LAYOUT_SHIFT_MAP 0
#endif

struct layout_shift_map_data_view {
    zmk_mod_flags_t modifier_map[8];
    bool active;
};

struct compact_modifier_state {
    uint8_t modifiers;
    bool windows_mode;
};

static bool compact_windows_mode(void) {
#if COMPACT_HAS_LAYOUT_SHIFT_MAP
    if (!device_is_ready(compact_layout_shift_map)) return false;
    const struct layout_shift_map_data_view *data = compact_layout_shift_map->data;
    return data->active;
#else
    return zmk_keymap_layer_active(HANA_WIN_BASE_LAYER);
#endif
}

static void compact_modifiers_update_cb(struct compact_modifier_state state) {
    zmk_compact_renderer_set_modifiers(state.modifiers, state.windows_mode);
}

static struct compact_modifier_state compact_modifiers_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return (struct compact_modifier_state) {
        .modifiers = zmk_hid_get_explicit_mods(),
        .windows_mode = compact_windows_mode(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(compact_modifiers, struct compact_modifier_state,
                            compact_modifiers_update_cb, compact_modifiers_get_state)
ZMK_SUBSCRIPTION(compact_modifiers, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(compact_modifiers, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(compact_modifiers, zmk_position_state_changed);

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent) {
    /* State is drawn by the shared 128x32 canvas, not a second LVGL layout. */
    widget->obj = parent;
    compact_modifiers_update_cb(compact_modifiers_get_state(NULL));
    compact_modifiers_init();
    return 0;
}

lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget) {
    return widget->obj;
}
