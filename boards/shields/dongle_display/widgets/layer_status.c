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
#include "battery_status.h"
#include <string.h>

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool base_text_enabled;

static bool is_compact_display(void) {
    lv_display_t *display = lv_display_get_default();
    return display != NULL && lv_display_get_vertical_resolution(display) <= 32;
}

static bool layer_is_base(struct layer_status_state state) {
    if (state.name != NULL && state.name[0] != '\0') {
        return strcmp(state.name, "BASE") == 0 ||
               strcmp(state.name, "Base") == 0;
    }

    /* Fall back to the conventional first layer only when no name is exposed. */
    return state.index == 0;
}

struct layer_status_state {
    uint8_t index;
    const char *name;
};

static const char *fallback_layer_name(uint8_t index) {
    switch (index) {
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

    const bool compact_display = is_compact_display();
    const bool base_layer = layer_is_base(state);

    if (compact_display) {
        /* On 128x32, batteries and layer name share the upper status zone. */
        zmk_widget_dongle_battery_status_set_hidden(!base_layer);
        if (base_layer) {
            lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
            return;
        }
    } else if (base_layer && !base_text_enabled) {
        /* Preserve the existing 128x64 BASE -> WPM -> Bongo Cat cycle. */
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const char *text = state.name;
    if (text == NULL || text[0] == '\0') {
        text = fallback_layer_name(state.index);
    }

    lv_label_set_text(widget->label, text);
    lv_label_set_text(widget->label_bold, text);

    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(widget->obj);
}

static void refresh_all(void) {
    uint8_t index = zmk_keymap_highest_layer_active();
    struct layer_status_state state = {
        .index = index,
        .name = zmk_keymap_layer_name(index),
    };

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

    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state) {
        .index = index,
        .name = zmk_keymap_layer_name(index),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget,
                                 lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    if (is_compact_display()) {
        /* Match the 78px battery column; centre a 14px label in its 24px zone. */
        lv_obj_set_size(widget->obj, 78, 14);
        lv_obj_align(widget->obj, LV_ALIGN_TOP_RIGHT, 0, 5);
    } else {
        lv_obj_set_size(widget->obj, 62, 26);
    }
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

    widget->layer = zmk_keymap_highest_layer_active();

    sys_slist_append(&widgets, &widget->node);

    set_layer_text(widget, (struct layer_status_state) {
        .index = widget->layer,
        .name = zmk_keymap_layer_name(widget->layer),
    });

    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) {
    return widget->obj;
}
