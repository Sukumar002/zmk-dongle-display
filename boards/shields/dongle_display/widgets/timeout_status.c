/*
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/runtime_timeout_changed.h>
#include "timeout_status.h"

/* Implemented by custom_status_screen.c for the compact 128x32 dashboard. */
void zmk_display_timeout_overlay_set(bool active);

static void restore_base_screen(lv_timer_t *timer) {
    struct zmk_widget_timeout_status *widget = lv_timer_get_user_data(timer);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
    zmk_display_timeout_overlay_set(false);
}

static void set_timeout_text(lv_obj_t *label, enum zmk_runtime_timeout_kind kind,
                             uint32_t timeout_ms) {
    const char *prefix = (kind == ZMK_RUNTIME_TIMEOUT_DISPLAY) ? "USB " : "KEY ";
    if (timeout_ms == 0) {
        lv_label_set_text_fmt(label, "%sNever", prefix);
    } else if (kind == ZMK_RUNTIME_TIMEOUT_DISPLAY) {
        if (timeout_ms == 300000) {
            lv_label_set_text_fmt(label, "%s300s", prefix);
        } else {
            lv_label_set_text_fmt(label, "%s%us", prefix, timeout_ms / 1000);
        }
    } else {
        lv_label_set_text_fmt(label, "%s%um", prefix, timeout_ms / 60000);
    }
}

struct timeout_state {
    enum zmk_runtime_timeout_kind kind;
    uint32_t timeout_ms;
    enum zmk_runtime_timeout_feedback feedback;
};

static struct timeout_state timeout_status_get_state(const zmk_event_t *eh) {
    const struct zmk_runtime_timeout_changed *ev = as_zmk_runtime_timeout_changed(eh);
    return (struct timeout_state) {
        .kind = ev->kind,
        .timeout_ms = ev->timeout_ms,
        .feedback = ev->feedback,
    };
}

static void timeout_status_update_cb(struct timeout_state state) {
    static struct zmk_widget_timeout_status *widget;
    ARG_UNUSED(widget);
    /* active_widget is assigned during widget initialization below. */
    extern struct zmk_widget_timeout_status *zmk_timeout_active_widget;
    widget = zmk_timeout_active_widget;
    if (widget == NULL) return;

    if (state.feedback == ZMK_RUNTIME_TIMEOUT_FEEDBACK_HIDE) {
        lv_timer_pause(widget->hide_timer);
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        zmk_display_timeout_overlay_set(false);
        return;
    }

    set_timeout_text(widget->label, state.kind, state.timeout_ms);
    zmk_display_timeout_overlay_set(true);
    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(widget->obj);

    if (state.feedback == ZMK_RUNTIME_TIMEOUT_FEEDBACK_HOLD) {
        lv_timer_pause(widget->hide_timer);
    } else {
        lv_timer_set_period(widget->hide_timer, 2500);
        lv_timer_reset(widget->hide_timer);
        lv_timer_resume(widget->hide_timer);
    }
}

struct zmk_widget_timeout_status *zmk_timeout_active_widget;

ZMK_DISPLAY_WIDGET_LISTENER(widget_timeout_status, struct timeout_state,
                            timeout_status_update_cb, timeout_status_get_state)
ZMK_SUBSCRIPTION(widget_timeout_status, zmk_runtime_timeout_changed);

int zmk_widget_timeout_status_init(struct zmk_widget_timeout_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 128, 32);
    lv_obj_set_style_bg_color(widget->obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(widget->obj, 0, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);

    widget->label = lv_label_create(widget->obj);
    lv_obj_set_width(widget->label, 128);
    lv_obj_set_style_text_align(widget->label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(widget->label);

    widget->hide_timer = lv_timer_create(restore_base_screen, 2500, widget);
    lv_timer_pause(widget->hide_timer);

    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    zmk_timeout_active_widget = widget;

    widget_timeout_status_init();
    return 0;
}

lv_obj_t *zmk_widget_timeout_status_obj(struct zmk_widget_timeout_status *widget) {
    return widget->obj;
}
