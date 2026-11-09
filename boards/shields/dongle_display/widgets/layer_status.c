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

    if (hana_base_view_is_base_layer(state.index) && !base_text_enabled) {
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


/* Full-panel layer-change notice for the compact 128x32 OLED. */
struct compact_layer_notice_state {
    uint8_t index;
    const char *name;
};

static lv_obj_t *compact_layer_notice;
static lv_obj_t *compact_layer_label;
static lv_obj_t *compact_layer_label_bold;
static lv_timer_t *compact_layer_timer;

static void compact_layer_notice_hide(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    if (compact_layer_notice != NULL) {
        lv_obj_add_flag(compact_layer_notice, LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_pause(compact_layer_timer);
}

static void compact_layer_notice_update(struct compact_layer_notice_state state) {
    if (compact_layer_notice == NULL) {
        return;
    }

    /*
     * Base is the idle/dashboard state, not a layer notification. Hide the
     * overlay immediately when the last momentary layer is released so the
     * dashboard or modifier-family view remains visible without a BASE flash.
     */
    /*
     * Hana's compact dongle keymap has exactly one base layer at index 0.
     * The shared base-layer-count helper also treats index 1 as a base layer
     * on multi-base layouts, but on Hana index 1 is the Num layer. Using that
     * helper here silently suppressed the Num notification.
     */
    if (state.index == 0) {
        lv_obj_add_flag(compact_layer_notice, LV_OBJ_FLAG_HIDDEN);
        if (compact_layer_timer != NULL) {
            lv_timer_pause(compact_layer_timer);
        }
        return;
    }

    const char *name = state.name;
    if (name == NULL || name[0] == '\\0') {
        name = fallback_layer_name(state.index);
    }

    lv_label_set_text(compact_layer_label, name);
    lv_label_set_text(compact_layer_label_bold, name);
    lv_obj_clear_flag(compact_layer_notice, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(compact_layer_notice);
    lv_timer_set_period(compact_layer_timer, 2500);
    lv_timer_reset(compact_layer_timer);
    lv_timer_resume(compact_layer_timer);
}

static struct compact_layer_notice_state compact_layer_notice_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    uint8_t index = zmk_keymap_highest_layer_active();
    const zmk_keymap_layer_id_t layer_id = zmk_keymap_layer_index_to_id(index);
    return (struct compact_layer_notice_state) {
        .index = index,
        .name = zmk_keymap_layer_name(layer_id),
    };
}

static void compact_layer_notice_update_cb(struct compact_layer_notice_state state) {
    compact_layer_notice_update(state);
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_compact_layer_notice,
                            struct compact_layer_notice_state,
                            compact_layer_notice_update_cb,
                            compact_layer_notice_get_state)
ZMK_SUBSCRIPTION(widget_compact_layer_notice, zmk_layer_state_changed);

int zmk_widget_layer_status_compact_init(lv_obj_t *parent) {
    compact_layer_notice = lv_obj_create(parent);
    lv_obj_set_size(compact_layer_notice, 128, 32);
    lv_obj_set_style_bg_color(compact_layer_notice, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(compact_layer_notice, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(compact_layer_notice, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(compact_layer_notice, 0, LV_PART_MAIN);

    lv_obj_t *frame = lv_obj_create(compact_layer_notice);
    lv_obj_set_size(frame, 116, 22);
    lv_obj_center(frame);
    lv_obj_set_style_bg_color(frame, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(frame, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(frame, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(frame, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_radius(frame, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(frame, 1, LV_PART_MAIN);

    compact_layer_label = lv_label_create(frame);
    compact_layer_label_bold = lv_label_create(frame);
    lv_obj_set_width(compact_layer_label, 108);
    lv_obj_set_width(compact_layer_label_bold, 108);
    lv_obj_set_style_text_align(compact_layer_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_align(compact_layer_label_bold, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(compact_layer_label, LV_ALIGN_CENTER, -1, 0);
    lv_obj_align(compact_layer_label_bold, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_long_mode(compact_layer_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_long_mode(compact_layer_label_bold, LV_LABEL_LONG_SCROLL_CIRCULAR);

    compact_layer_timer = lv_timer_create(compact_layer_notice_hide, 2500, NULL);
    lv_timer_pause(compact_layer_timer);
    lv_obj_add_flag(compact_layer_notice, LV_OBJ_FLAG_HIDDEN);
    widget_compact_layer_notice_init();
    return 0;
}
