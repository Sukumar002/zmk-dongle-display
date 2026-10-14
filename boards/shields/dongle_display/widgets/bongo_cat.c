/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/wpm.h>

#include "bongo_cat.h"

#define SRC(array) (const void **)array, sizeof(array) / sizeof(lv_img_dsc_t *)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool base_cat_enabled = true;
static int64_t last_anim_update_time = 0;
#define ANIM_UPDATE_INTERVAL_MS 200  // Throttle: max 5 animation checks per second

LV_IMG_DECLARE(bongo_cat_none);
LV_IMG_DECLARE(bongo_cat_left1);
LV_IMG_DECLARE(bongo_cat_left2);
LV_IMG_DECLARE(bongo_cat_right1);
LV_IMG_DECLARE(bongo_cat_right2);
LV_IMG_DECLARE(bongo_cat_both1);
LV_IMG_DECLARE(bongo_cat_both1_open);
LV_IMG_DECLARE(bongo_cat_both2);

#define ANIMATION_SPEED_IDLE 10000
const lv_img_dsc_t *idle_imgs[] = {
    &bongo_cat_both1_open,
    &bongo_cat_both1_open,
    &bongo_cat_both1_open,
    &bongo_cat_both1,
};

#define ANIMATION_SPEED_SLOW 2000
const lv_img_dsc_t *slow_imgs[] = {
    &bongo_cat_left1,
    &bongo_cat_both1,
    &bongo_cat_both1,
    &bongo_cat_right1,
    &bongo_cat_both1,
    &bongo_cat_both1,
    &bongo_cat_left1,
    &bongo_cat_both1,
    &bongo_cat_both1,
};

#define ANIMATION_SPEED_MID 500
const lv_img_dsc_t *mid_imgs[] = {
    &bongo_cat_left2,
    &bongo_cat_left1,
    &bongo_cat_none,
    &bongo_cat_right2,
    &bongo_cat_right1,
    &bongo_cat_none,
};

#define ANIMATION_SPEED_FAST 200
const lv_img_dsc_t *fast_imgs[] = {
    &bongo_cat_both2,
    &bongo_cat_both1,
    &bongo_cat_none,
    &bongo_cat_none,
};

struct bongo_cat_wpm_status_state {
    uint8_t wpm;
};

enum anim_state {
    anim_state_none,
    anim_state_idle,
    anim_state_slow,
    anim_state_mid,
    anim_state_fast
} current_anim_state;

static void set_animation(lv_obj_t *animing, struct bongo_cat_wpm_status_state state) {
    // Throttle animation state changes to prevent display thread flooding
    int64_t now = k_uptime_get();
    if ((now - last_anim_update_time) < ANIM_UPDATE_INTERVAL_MS) {
        return;
    }
    last_anim_update_time = now;

    if (state.wpm < 5) {
        if (current_anim_state != anim_state_idle) {
            lv_animimg_set_src(animing, SRC(idle_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_IDLE);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_idle;
        }
    } else if (state.wpm < 30) {
        if (current_anim_state != anim_state_slow) {
            lv_animimg_set_src(animing, SRC(slow_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SLOW);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_slow;
        }
    } else if (state.wpm < 70) {
        if (current_anim_state != anim_state_mid) {
            lv_animimg_set_src(animing, SRC(mid_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_MID);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_mid;
        }
    } else {
        if (current_anim_state != anim_state_fast) {
            lv_animimg_set_src(animing, SRC(fast_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_FAST);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_fast;
        }
    }
}

struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    // Add NULL check to prevent crash if event is NULL
    return (struct bongo_cat_wpm_status_state) { .wpm = ev ? ev->state : 0 };
};

void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (zmk_keymap_highest_layer_active() == 0 && base_cat_enabled) {
            lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
            set_animation(widget->obj, state);
        }
    }
}

static int bongo_cat_layer_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    const bool base = zmk_keymap_highest_layer_active() == 0;
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (base && base_cat_enabled) {
            current_anim_state = anim_state_none;
            lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
            set_animation(widget->obj, (struct bongo_cat_wpm_status_state){ .wpm = zmk_wpm_get_state() });
        } else {
            /*
             * LVGL 9 does not expose lv_animimg_stop() in this build.
             * Deleting animations attached to the animimg object stops the
             * current Bongo Cat frame animation before hiding it.
             */
            lv_anim_del(widget->obj, NULL);
            lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_bongo_cat_layer, bongo_cat_layer_listener);
ZMK_SUBSCRIPTION(widget_bongo_cat_layer, zmk_layer_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state,
                            bongo_cat_wpm_status_update_cb, bongo_cat_wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    lv_obj_center(widget->obj);

    sys_slist_append(&widgets, &widget->node);

    if (!base_cat_enabled || zmk_keymap_highest_layer_active() != 0) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    }

    widget_bongo_cat_init();

    return 0;
}

void zmk_widget_bongo_cat_set_base_enabled(bool enabled) {
    base_cat_enabled = enabled;

    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (enabled && zmk_keymap_highest_layer_active() == 0) {
            current_anim_state = anim_state_none;
            lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
            set_animation(widget->obj,
                          (struct bongo_cat_wpm_status_state){ .wpm = zmk_wpm_get_state() });
        } else {
            lv_anim_del(widget->obj, NULL);
            lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}