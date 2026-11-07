/*
 * Copyright (c) 2024 The ZMK Contributors
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

#include "modifiers.h"

struct compact_modifier_state { uint8_t modifiers; };

static struct zmk_widget_modifiers *active_widget;
static lv_obj_t *cells[4];
static lv_obj_t *labels[4];

static const uint8_t modifier_masks[4] = {
    MOD_LCTL | MOD_RCTL,
    MOD_LALT | MOD_RALT,
    MOD_LGUI | MOD_RGUI,
    MOD_LSFT | MOD_RSFT,
};

static const char *const modifier_names[4] = {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MAC_MODIFIERS)
    "CTRL", "OPT", "CMD", "SHIFT"
#else
    "CTRL", "ALT", "GUI", "SHIFT"
#endif
};

static void compact_modifiers_update(struct compact_modifier_state state) {
    for (int i = 0; i < 4; i++) {
        bool pressed = (state.modifiers & modifier_masks[i]) != 0;
        lv_obj_set_style_bg_color(cells[i], pressed ? lv_color_black() : lv_color_white(), 0);
        lv_obj_set_style_text_color(labels[i], pressed ? lv_color_white() : lv_color_black(), 0);
    }
}

static void compact_modifiers_update_cb(struct compact_modifier_state state) {
    ARG_UNUSED(active_widget);
    compact_modifiers_update(state);
}

static struct compact_modifier_state compact_modifiers_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return (struct compact_modifier_state) { .modifiers = zmk_hid_get_explicit_mods() };
}

ZMK_DISPLAY_WIDGET_LISTENER(compact_modifiers, struct compact_modifier_state,
                            compact_modifiers_update_cb, compact_modifiers_get_state)
ZMK_SUBSCRIPTION(compact_modifiers, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(compact_modifiers, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(compact_modifiers, zmk_position_state_changed);

int zmk_widget_modifiers_init(struct zmk_widget_modifiers *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    widget->node = (sys_snode_t){0};
    active_widget = widget;
    lv_obj_set_size(widget->obj, 128, 32);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);

    for (int i = 0; i < 4; i++) {
        cells[i] = lv_obj_create(widget->obj);
        lv_obj_set_size(cells[i], 31, 32);
        lv_obj_set_style_radius(cells[i], 0, 0);
        lv_obj_set_style_border_width(cells[i], 1, 0);
        lv_obj_set_style_border_color(cells[i], lv_color_black(), 0);
        lv_obj_set_style_bg_opa(cells[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(cells[i], 0, 0);
        lv_obj_set_pos(cells[i], i * 32, 0);
        labels[i] = lv_label_create(cells[i]);
        lv_label_set_text(labels[i], modifier_names[i]);
        lv_obj_center(labels[i]);
    }
    compact_modifiers_update(compact_modifiers_get_state(NULL));
    compact_modifiers_init();
    return 0;
}

lv_obj_t *zmk_widget_modifiers_obj(struct zmk_widget_modifiers *widget) {
    return widget->obj;
}
