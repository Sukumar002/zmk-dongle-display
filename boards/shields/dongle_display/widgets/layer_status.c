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

static void draw_gear(lv_obj_t *c) {
    clear(c);

    const int cx = 16, cy = 12;

    /*
     * Static settings gear, optimized for the 32x24 monochrome canvas.
     * A solid, symmetric silhouette reads much more clearly at this size
     * than the previous animated tooth approximation.
     */

    /* Outer teeth. */
    rect(c, cx - 2, 2, 5, 3);
    rect(c, cx - 2, 19, 5, 3);
    rect(c, 6, cy - 2, 3, 5);
    rect(c, 23, cy - 2, 3, 5);

    rect(c, 8, 4, 4, 4);
    rect(c, 20, 4, 4, 4);
    rect(c, 8, 16, 4, 4);
    rect(c, 20, 16, 4, 4);

    /* Main gear body. */
    rect(c, 11, 6, 11, 13);
    rect(c, 9, 8, 15, 9);

    /* Hollow centre. */
    rect(c, 14, 10, 5, 5);
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
        draw_gear(widget->obj);
        break;
    case 4:
        draw_mouse(widget->obj);
        break;
    default:
        clear(widget->obj);
        break;
    }
}


struct layer_status_state {
    uint8_t index;
};

static void set_layer_symbol(struct zmk_widget_layer_status *widget,
                             struct layer_status_state state) {
    widget->layer = state.index;

    if (state.index == 0) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    redraw(widget);
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
    widget->layer = zmk_keymap_highest_layer_active();

    sys_slist_append(&widgets, &widget->node);

    if (widget->layer == 0) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        redraw(widget);
    }

    widget_layer_status_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget) {
    return widget->obj;
}
