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

static bool layer_is_base(struct layer_status_state state) {
    /* Use the actual keymap layer name, not its display index. Studio can
     * reorder layers, and Num must never be mistaken for the base layer. */
    if (state.name != NULL && state.name[0] != '\0') {
        return strcmp(state.name, "BASE") == 0 ||
               strcmp(state.name, "Base") == 0;
    }

    /* Only use the conventional first-layer fallback when no name exists. */
    return state.index == 0;
}

static void set_layer_text(struct zmk_widget_layer_status *widget,
                           struct layer_status_state state) {
    widget->layer = state.index;
    const bool base_layer = layer_is_base(state);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    /* The 32px base view is always the battery panel. Never draw BASE over it,
     * even while the user cycles the 64px base-view modes. */
    zmk_widget_dongle_battery_status_set_hidden(!base_layer);
    if (base_layer) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
#else
    /* On 64px displays, BASE participates in the existing Base/WPM/Cat cycle. */
    if (base_layer && !base_text_enabled) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }
#endif

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
    /* highest_layer_active() returns a display/order index, while
     * zmk_keymap_layer_name() takes the stable layer ID. They differ when
     * ZMK Studio reorders layers. */
    const zmk_keymap_layer_id_t layer_id = zmk_keymap_layer_index_to_id(index);
    struct layer_status_state state = {
        .index = index,
        .name = zmk_keymap_layer_name(layer_id),
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
    const zmk_keymap_layer_id_t layer_id = zmk_keymap_layer_index_to_id(index);
    return (struct layer_status_state) {
        .index = index,
        .name = zmk_keymap_layer_name(layer_id),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget,
                                 lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    /* Right-hand half only: x=64..127 on the 128px display. */
    lv_obj_set_size(widget->obj, 64, 24);
#else
    lv_obj_set_size(widget->obj, 62, 26);
#endif
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 1, 0);
    lv_obj_set_style_border_color(widget->obj, lv_color_black(), 0);
    lv_obj_set_style_radius(widget->obj, 2, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);

    widget->label = lv_label_create(widget->obj);
    widget->label_bold = lv_label_create(widget->obj);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    lv_obj_set_width(widget->label, 62);
    lv_obj_set_width(widget->label_bold, 62);
    lv_label_set_long_mode(widget->label, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(widget->label_bold, LV_LABEL_LONG_DOT);
#else
    lv_obj_set_width(widget->label, 60);
    lv_obj_set_width(widget->label_bold, 60);
#endif
    lv_obj_set_style_text_align(widget->label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_align(widget->label_bold, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_align(widget->label, LV_ALIGN_CENTER, -1, 0);
    lv_obj_align(widget->label_bold, LV_ALIGN_CENTER, 0, 0);

    widget->layer = zmk_keymap_highest_layer_active();

    sys_slist_append(&widgets, &widget->node);

    const zmk_keymap_layer_id_t layer_id =
        zmk_keymap_layer_index_to_id(widget->layer);
    set_layer_text(widget, (struct layer_status_state) {
        .index = widget->layer,
        .name = zmk_keymap_layer_name(layer_id),
    });

    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) {
    return widget->obj;
}
