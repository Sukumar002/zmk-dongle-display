/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include "modifiers_compact.h"

/*
 * Read the same persistent GUI/Alt layout-shift behavior used by the
 * keyboard keymap. Its active state means Windows; inactive means macOS.
 * Keep the legacy layer check only as a fallback for configurations that
 * do not provide the shared layout-shift device.
 */
#define HANA_WIN_BASE_LAYER 1

#if DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt_shared))
#define HANA_HAS_GUI_ALT_LAYOUT_SHIFT 1
static const struct device *const hana_gui_alt_layout_shift =
    DEVICE_DT_GET(DT_NODELABEL(layout_shift_map_gui_alt_shared));

struct hana_layout_shift_map_data_view {
    zmk_mod_flags_t modifier_map[8];
    bool active;
};

static bool hana_layout_shift_windows_mode(void) {
    if (!device_is_ready(hana_gui_alt_layout_shift)) {
        return false;
    }

    const struct hana_layout_shift_map_data_view *data =
        hana_gui_alt_layout_shift->data;
    return data->active;
}
#else
#define HANA_HAS_GUI_ALT_LAYOUT_SHIFT 0
#endif

struct compact_layout_state {
    bool windows_mode;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void set_layout_label(
    struct zmk_widget_compact_modifiers *widget,
    struct compact_layout_state state
) {
    lv_label_set_text(widget->label, state.windows_mode ? "WIN" : "MAC");
}

static void compact_layout_update_cb(struct compact_layout_state state) {
    struct zmk_widget_compact_modifiers *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layout_label(widget, state);
    }
}

static struct compact_layout_state compact_layout_get_state(const zmk_event_t *eh) {
#if HANA_HAS_GUI_ALT_LAYOUT_SHIFT
    bool windows_mode = hana_layout_shift_windows_mode();
#else
    bool windows_mode = zmk_keymap_layer_active(HANA_WIN_BASE_LAYER);
#endif

    return (struct compact_layout_state) {
        .windows_mode = windows_mode,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(
    widget_compact_layout,
    struct compact_layout_state,
    compact_layout_update_cb,
    compact_layout_get_state
)

ZMK_SUBSCRIPTION(widget_compact_layout, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(widget_compact_layout, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(widget_compact_layout, zmk_position_state_changed);

int zmk_widget_compact_modifiers_init(
    struct zmk_widget_compact_modifiers *widget,
    lv_obj_t *parent
) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 24, 9);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    widget->label = lv_label_create(widget->obj);
    lv_obj_set_style_text_font(widget->label, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(widget->label, 1, LV_PART_MAIN);
    lv_label_set_long_mode(widget->label, LV_LABEL_LONG_CLIP);
    lv_obj_align(widget->label, LV_ALIGN_LEFT_MID, 0, 0);

    set_layout_label(widget, compact_layout_get_state(NULL));

    sys_slist_append(&widgets, &widget->node);
    widget_compact_layout_init();

    return 0;
}

lv_obj_t *zmk_widget_compact_modifiers_obj(
    struct zmk_widget_compact_modifiers *widget
) {
    return widget->obj;
}
