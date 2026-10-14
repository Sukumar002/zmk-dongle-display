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

#define ICON_W 32
#define ICON_H 24
#define ICON_BUF_SIZE LV_CANVAS_BUF_SIZE(ICON_W, ICON_H, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static lv_color_t icon_buffers[1][ICON_BUF_SIZE];

static void clear(lv_obj_t *canvas) {
    /*
     * Match the normal white OLED background. Icons are drawn as black
     * outlines instead of white/negative shapes inside a dark block.
     */
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
}

static void px(lv_obj_t *canvas, int x, int y) {
    if (x >= 0 && x < ICON_W && y >= 0 && y < ICON_H) {
        lv_canvas_set_px(canvas, x, y, lv_color_black(), LV_OPA_COVER);
    }
}

static void hline(lv_obj_t *c, int x0, int x1, int y) {
    for (int x = x0; x <= x1; x++) px(c, x, y);
}

static void vline(lv_obj_t *c, int x, int y0, int y1) {
    for (int y = y0; y <= y1; y++) px(c, x, y);
}

static void rect(lv_obj_t *c, int x, int y, int w, int h) {
    hline(c, x, x + w - 1, y);
    hline(c, x, x + w - 1, y + h - 1);
    vline(c, x, y, y + h - 1);
    vline(c, x + w - 1, y, y + h - 1);
}

static void draw_num(lv_obj_t *c) {
    clear(c);
    /* Compact calculator/numpad. */
    rect(c, 6, 1, 20, 22);
    rect(c, 9, 3, 14, 5);

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            rect(c, 9 + col * 5, 10 + row * 4, 3, 3);
        }
    }
}

static void draw_arrow_key(lv_obj_t *c, int x, int y, char dir) {
    rect(c, x, y, 8, 8);

    int cx = x + 3;
    int cy = y + 3;
    switch (dir) {
    case 'U':
        px(c, cx, cy - 2); px(c, cx - 1, cy - 1); px(c, cx + 1, cy - 1);
        vline(c, cx, cy - 1, cy + 2);
        break;
    case 'D':
        px(c, cx, cy + 2); px(c, cx - 1, cy + 1); px(c, cx + 1, cy + 1);
        vline(c, cx, cy - 2, cy + 1);
        break;
    case 'L':
        px(c, cx - 2, cy); px(c, cx - 1, cy - 1); px(c, cx - 1, cy + 1);
        hline(c, cx - 1, cx + 2, cy);
        break;
    case 'R':
        px(c, cx + 2, cy); px(c, cx + 1, cy - 1); px(c, cx + 1, cy + 1);
        hline(c, cx - 2, cx + 1, cy);
        break;
    }
}

static void draw_nav(lv_obj_t *c) {
    clear(c);
    /* Standard inverted-T arrow cluster: Up above Down, L/D/R below. */
    draw_arrow_key(c, 12, 1, 'U');
    draw_arrow_key(c, 3, 10, 'L');
    draw_arrow_key(c, 12, 10, 'D');
    draw_arrow_key(c, 21, 10, 'R');
}

static void draw_mouse(lv_obj_t *c) {
    clear(c);

    /* 18x22 mouse outline centered in the widget. */
    rect(c, 7, 1, 18, 22);
    vline(c, 16, 2, 7);
    hline(c, 8, 24, 8);

    /* Rounded-ish lower body and wheel. */
    px(c, 8, 22); px(c, 24, 22);
    px(c, 9, 23); px(c, 23, 23);
    rect(c, 14, 3, 5, 4);
}

static void draw_gear(lv_obj_t *c, uint8_t phase) {
    clear(c);

    const int cx = 16, cy = 12;

    /* Hollow centre and compact outline ring. */
    rect(c, cx - 5, cy - 5, 11, 11);
    rect(c, cx - 2, cy - 2, 5, 5);

    /*
     * Eight-frame tooth pattern. The body stays constant while the outer
     * teeth advance around it, giving a clear rotating settings-gear effect.
     */
    static const int8_t teeth[8][8][2] = {
        {{ 0,-8},{ 5,-6},{ 8, 0},{ 5, 6},{ 0, 8},{-5, 6},{-8, 0},{-5,-6}},
        {{ 2,-8},{ 6,-5},{ 8, 2},{ 5, 6},{-2, 8},{-6, 5},{-8,-2},{-5,-6}},
        {{ 4,-7},{ 7,-4},{ 7, 4},{ 4, 7},{-4, 7},{-7, 4},{-7,-4},{-4,-7}},
        {{ 5,-6},{ 8,-2},{ 6, 5},{ 2, 8},{-5, 6},{-8, 2},{-6,-5},{-2,-8}},
        {{ 0,-8},{ 6,-5},{ 8, 0},{ 6, 5},{ 0, 8},{-6, 5},{-8, 0},{-6,-5}},
        {{-2,-8},{ 5,-6},{ 8,-2},{ 6, 5},{ 2, 8},{-5, 6},{-8, 2},{-6,-5}},
        {{-4,-7},{ 4,-7},{ 7,-4},{ 7, 4},{ 4, 7},{-4, 7},{-7, 4},{-7,-4}},
        {{-5,-6},{ 2,-8},{ 6,-5},{ 8, 2},{ 5, 6},{-2, 8},{-6, 5},{-8,-2}},
    };

    uint8_t f = phase & 7;
    for (int i = 0; i < 8; i++) {
        int x = cx + teeth[f][i][0];
        int y = cy + teeth[f][i][1];
        px(c, x, y);

        /* Give cardinal/near-cardinal teeth a second pixel for readability. */
        if (teeth[f][i][0] == 0 || teeth[f][i][1] == 0) {
            if (teeth[f][i][0] == 0) px(c, x + 1, y);
            if (teeth[f][i][1] == 0) px(c, x, y + 1);
        }
    }
}

static void redraw(struct zmk_widget_layer_status *widget) {
    switch (widget->layer) {
    case 1:
        draw_num(widget->obj);
        break;
    case 2:
        draw_nav(widget->obj);
        break;
    case 3:
        draw_gear(widget->obj, widget->gear_phase);
        break;
    case 4:
        draw_mouse(widget->obj);
        break;
    default:
        clear(widget->obj);
        break;
    }
}

static void gear_timer_cb(lv_timer_t *timer) {
    struct zmk_widget_layer_status *widget = lv_timer_get_user_data(timer);
    if (widget->layer != 3) {
        lv_timer_pause(timer);
        return;
    }

    widget->gear_phase = (widget->gear_phase + 1) & 7;
    redraw(widget);
}

struct layer_status_state {
    uint8_t index;
};

static void set_layer_symbol(struct zmk_widget_layer_status *widget,
                             struct layer_status_state state) {
    widget->layer = state.index;

    if (state.index == 0) {
        lv_timer_pause(widget->anim_timer);
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);

    if (state.index == 3) {
        widget->gear_phase = 0;
        redraw(widget);
        lv_timer_set_period(widget->anim_timer, 140);
        lv_timer_reset(widget->anim_timer);
        lv_timer_resume(widget->anim_timer);
    } else {
        lv_timer_pause(widget->anim_timer);
        redraw(widget);
    }
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_layer_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_layer_symbol(widget, state);
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);
    return (struct layer_status_state) {
        .index = zmk_keymap_highest_layer_active(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state,
                            layer_status_update_cb, layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent) {
    widget->obj = lv_canvas_create(parent);
    lv_canvas_set_buffer(widget->obj, icon_buffers[0], ICON_W, ICON_H, LV_COLOR_FORMAT_L8);
    widget->gear_phase = 0;
    widget->layer = zmk_keymap_highest_layer_active();

    widget->anim_timer = lv_timer_create(gear_timer_cb, 140, widget);
    lv_timer_pause(widget->anim_timer);

    sys_slist_append(&widgets, &widget->node);

    if (widget->layer == 0) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        redraw(widget);
        if (widget->layer == 3) {
            lv_timer_resume(widget->anim_timer);
        }
    }

    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) {
    return widget->obj;
}
