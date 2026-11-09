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
#include <zmk/events/hid_indicators_changed.h>

#include "hid_indicators.h"

#define LED_NLCK 0x01
#define LED_CLCK 0x02
#define LED_SLCK 0x04

struct hid_indicators_state {
    uint8_t indicators;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static void set_badge(lv_obj_t *badge, char letter, bool active) {
    if (!active) {
        lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char text[] = { letter, '\0' };
    lv_label_set_text(badge, text);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_HIDDEN);
}

static void set_hid_indicators(struct zmk_widget_hid_indicators *widget,
                               struct hid_indicators_state state) {
    const bool caps = (state.indicators & LED_CLCK) != 0;
    const bool num = (state.indicators & LED_NLCK) != 0;
    const bool scroll = (state.indicators & LED_SLCK) != 0;
    const unsigned int count = (caps ? 1 : 0) + (num ? 1 : 0) + (scroll ? 1 : 0);

    set_badge(widget->badges[0], 'C', caps);
    set_badge(widget->badges[1], 'N', num);
    set_badge(widget->badges[2], 'S', scroll);

    if (count == 0) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    /* Right-align compact active badges inside the fixed-width container. */
    const int badge_width = 9;
    const int gap = 2;
    int x = 32 - (int)count * badge_width - ((int)count - 1) * gap;
    for (int i = 0; i < 3; i++) {
        if (lv_obj_has_flag(widget->badges[i], LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        lv_obj_set_pos(widget->badges[i], x, 0);
        x += badge_width + gap;
    }
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
}

void hid_indicators_update_cb(struct hid_indicators_state state) {
    struct zmk_widget_hid_indicators *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_hid_indicators(widget, state);
    }
}

static struct hid_indicators_state hid_indicators_get_state(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    return (struct hid_indicators_state) { .indicators = ev->indicators };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_hid_indicators, struct hid_indicators_state,
                            hid_indicators_update_cb, hid_indicators_get_state)

ZMK_SUBSCRIPTION(widget_hid_indicators, zmk_hid_indicators_changed);

int zmk_widget_hid_indicators_init(struct zmk_widget_hid_indicators *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 32, 9);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);

    static const char letters[3] = { 'C', 'N', 'S' };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *badge = lv_label_create(widget->obj);
        widget->badges[i] = badge;
        lv_obj_set_size(badge, 9, 9);
        lv_obj_set_style_border_width(badge, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(badge, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_radius(badge, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(badge, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(badge, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_text_align(badge, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_text_fmt(badge, "%c", letters[i]);
        lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    }

    sys_slist_append(&widgets, &widget->node);
    widget_hid_indicators_init();
    return 0;
}

lv_obj_t *zmk_widget_hid_indicators_obj(struct zmk_widget_hid_indicators *widget) {
    return widget->obj;
}
