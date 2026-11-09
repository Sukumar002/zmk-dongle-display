/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <ctype.h>
#include <string.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/runtime_timeout_changed.h>

#include "timeout_status.h"

#define ICON_W 12
#define ICON_H 8
#define ICON_BUF_SIZE LV_CANVAS_BUF_SIZE(ICON_W, ICON_H, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)

static lv_color_t icon_buf[ICON_BUF_SIZE];
static struct zmk_widget_timeout_status *active_widget;

static void clear_icon(lv_obj_t *canvas) {
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);
}

static void px(lv_obj_t *canvas, int x, int y) {
    lv_canvas_set_px(canvas, x, y, lv_color_white(), LV_OPA_COVER);
}

static void draw_keyboard_icon(lv_obj_t *canvas) {
    clear_icon(canvas);

    for (int x = 0; x < 12; x++) {
        px(canvas, x, 0);
        px(canvas, x, 7);
    }
    for (int y = 1; y < 7; y++) {
        px(canvas, 0, y);
        px(canvas, 11, y);
    }

    for (int x = 2; x <= 9; x += 2) {
        px(canvas, x, 2);
        px(canvas, x, 4);
    }
    for (int x = 3; x <= 8; x++) {
        px(canvas, x, 6);
    }
}

static void draw_usb_icon(lv_obj_t *canvas) {
    clear_icon(canvas);

    /* Landscape USB plug, sized to the original 12x8 timeout icon footprint. */
    px(canvas, 0, 3);
    px(canvas, 1, 3);
    px(canvas, 2, 3);
    px(canvas, 2, 4);
    px(canvas, 1, 4);
    px(canvas, 0, 4);

    for (int x = 3; x <= 9; x++) {
        px(canvas, x, 1);
        px(canvas, x, 6);
    }
    for (int y = 2; y <= 5; y++) {
        px(canvas, 3, y);
        px(canvas, 9, y);
    }

    px(canvas, 10, 2);
    px(canvas, 11, 2);
    px(canvas, 10, 5);
    px(canvas, 11, 5);
    px(canvas, 7, 3);
    px(canvas, 8, 3);
    px(canvas, 7, 4);
    px(canvas, 8, 4);
}

static void hide_timer_cb(lv_timer_t *timer) {
    struct zmk_widget_timeout_status *widget = lv_timer_get_user_data(timer);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(timer);
}

static void set_timeout_text(lv_obj_t *label, enum zmk_runtime_timeout_kind kind,
                             uint32_t timeout_ms) {
    if (timeout_ms == 0) {
        lv_label_set_text(label, "Never");
        return;
    }

    if (kind == ZMK_RUNTIME_TIMEOUT_DISPLAY) {
        if (timeout_ms == 300000) {
            lv_label_set_text(label, "300s");
        } else {
            lv_label_set_text_fmt(label, "%us", timeout_ms / 1000);
        }
    } else {
        lv_label_set_text_fmt(label, "%um", timeout_ms / 60000);
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
    if (active_widget == NULL) {
        return;
    }

    if (state.feedback == ZMK_RUNTIME_TIMEOUT_FEEDBACK_HIDE) {
        lv_timer_pause(active_widget->hide_timer);
        lv_obj_add_flag(active_widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    /*
     * ZMK's initial display idle timeout is 2 seconds. It is an initialization
     * value, not a user-selected timeout, so don't show it over the startup
     * connection indicators. The Hana timeout behavior offers 30s/60s/300s/
     * Never, so 2s is not a selectable value.
     */
    if (state.kind == ZMK_RUNTIME_TIMEOUT_DISPLAY && state.timeout_ms == 2000) {
        return;
    }
#endif

    /* Runtime timeout feedback keeps its icon; board-name feedback is text-only. */
    lv_obj_clear_flag(active_widget->icon, LV_OBJ_FLAG_HIDDEN);
    if (state.kind == ZMK_RUNTIME_TIMEOUT_DISPLAY) {
        draw_usb_icon(active_widget->icon);
    } else {
        draw_keyboard_icon(active_widget->icon);
    }

    set_timeout_text(active_widget->label, state.kind, state.timeout_ms);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    /* Centre the icon + value as one group inside the opaque full-screen overlay. */
    lv_obj_update_layout(active_widget->label);
    lv_coord_t label_width = lv_obj_get_width(active_widget->label);
    lv_coord_t group_width = ICON_W + 4 + label_width;
    lv_coord_t screen_width = lv_obj_get_width(active_widget->obj);
    lv_coord_t screen_height = lv_obj_get_height(active_widget->obj);
    lv_obj_set_pos(active_widget->icon, (screen_width - group_width) / 2,
                   (screen_height - ICON_H) / 2);
    lv_obj_align_to(active_widget->label, active_widget->icon,
                    LV_ALIGN_OUT_RIGHT_MID, 4, 0);
#else
    lv_obj_align(active_widget->icon, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align_to(active_widget->label, active_widget->icon,
                    LV_ALIGN_OUT_RIGHT_MID, 4, 0);
#endif

    lv_obj_clear_flag(active_widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(active_widget->obj);

    if (state.feedback == ZMK_RUNTIME_TIMEOUT_FEEDBACK_HOLD) {
        /* Hold-to-view remains visible only for as long as the key is held. */
        lv_timer_pause(active_widget->hide_timer);
    } else {
        /* A changed value remains visible briefly after a normal tap. */
        lv_timer_set_period(active_widget->hide_timer, 3000);
        lv_timer_reset(active_widget->hide_timer);
        lv_timer_resume(active_widget->hide_timer);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_timeout_status, struct timeout_state,
                            timeout_status_update_cb, timeout_status_get_state)
ZMK_SUBSCRIPTION(widget_timeout_status, zmk_runtime_timeout_changed);

int zmk_widget_timeout_status_init(struct zmk_widget_timeout_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    /* An opaque, full-panel overlay hides every dashboard element during feedback. */
    lv_obj_set_size(widget->obj, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_align(widget->obj, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(widget->obj, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);
#else
    lv_obj_set_size(widget->obj, 112, 14);
#endif

    widget->icon = lv_canvas_create(widget->obj);
    lv_canvas_set_buffer(widget->icon, icon_buf, ICON_W, ICON_H, LV_COLOR_FORMAT_L8);
#if !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
    lv_obj_align(widget->icon, LV_ALIGN_LEFT_MID, 0, 0);
#endif

    widget->label = lv_label_create(widget->obj);
    lv_obj_align_to(widget->label, widget->icon, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    widget->hide_timer = lv_timer_create(hide_timer_cb, 2500, widget);
    lv_timer_pause(widget->hide_timer);

    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    active_widget = widget;

    widget_timeout_status_init();
    return 0;
}

lv_obj_t *zmk_widget_timeout_status_obj(struct zmk_widget_timeout_status *widget) {
    return widget->obj;
}

void zmk_widget_timeout_status_show_keyboard_name(const char *name) {
    if (active_widget == NULL || name == NULL) {
        return;
    }

    char uppercase_name[32];
    size_t i = 0;
    while (name[i] != '\0' && i < sizeof(uppercase_name) - 1) {
        uppercase_name[i] = (char)toupper((unsigned char)name[i]);
        i++;
    }
    uppercase_name[i] = '\0';

    /* The board switch banner is just the uppercase name; no keyboard glyph. */
    lv_obj_add_flag(active_widget->icon, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(active_widget->label, uppercase_name);
    lv_obj_align(active_widget->label, LV_ALIGN_LEFT_MID, 0, 0);
    /* Inherit the screen's compact unscii_8 font to prevent long names clipping. */
    lv_obj_clear_flag(active_widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(active_widget->obj);
    lv_timer_set_period(active_widget->hide_timer, 2500);
    lv_timer_reset(active_widget->hide_timer);
    lv_timer_resume(active_widget->hide_timer);
}
