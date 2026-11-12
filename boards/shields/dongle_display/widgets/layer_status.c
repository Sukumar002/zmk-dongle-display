/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include "layer_status.h"
#include <hana_base_view.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool base_text_enabled;

struct layer_status_state {
    /* Runtime position and stable firmware identity are not interchangeable. */
    uint8_t index;
    uint8_t layer_id;
    const char *name;
};

static const char *fallback_layer_name(uint8_t layer_id) {
    switch (layer_id) {
    case 0: return "BASE";
    case 1: return "NUM";
    case 2: return "NAV";
    case 3: return "SET";
    case 4: return "MOUSE";
    default: return "LAYER";
    }
}

static void set_layer_text(struct zmk_widget_layer_status *widget,
                           struct layer_status_state state) {
    widget->layer = state.index;

    if (hana_base_view_is_base_layer(state.index) && !base_text_enabled) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const char *text = state.name;
    if (text == NULL || text[0] == '\0') {
        text = fallback_layer_name(state.layer_id);
    }

    lv_label_set_text(widget->label, text);
    lv_label_set_text(widget->label_bold, text);

    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(widget->obj);
}

static struct layer_status_state current_layer_status_state(void) {
    uint8_t index = zmk_keymap_highest_layer_active();
    uint8_t layer_id = zmk_keymap_layer_index_to_id(index);

    return (struct layer_status_state) {
        .index = index,
        .layer_id = layer_id,
        .name = zmk_keymap_layer_name(layer_id),
    };
}

static void refresh_all(void) {
    struct layer_status_state state = current_layer_status_state();

    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_text(widget, state);
    }
}

void zmk_widget_layer_status_set_base_text_enabled(bool enabled) {
    base_text_enabled = enabled;
    refresh_all();
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_text(widget, state);
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return current_layer_status_state();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget,
                                 lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 62, 26);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 1, 0);
    lv_obj_set_style_border_color(widget->obj, lv_color_black(), 0);
    lv_obj_set_style_radius(widget->obj, 2, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);

    widget->label = lv_label_create(widget->obj);
    widget->label_bold = lv_label_create(widget->obj);

    lv_obj_set_width(widget->label, 60);
    lv_obj_set_width(widget->label_bold, 60);
    lv_obj_set_style_text_align(widget->label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(widget->label_bold, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_align(widget->label, LV_ALIGN_CENTER, -1, 0);
    lv_obj_align(widget->label_bold, LV_ALIGN_CENTER, 0, 0);

    struct layer_status_state state = current_layer_status_state();
    widget->layer = state.index;

    sys_slist_append(&widgets, &widget->node);

    set_layer_text(widget, state);

    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) {
    return widget->obj;
}
