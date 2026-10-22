/*
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_timeout_status {
    lv_obj_t *obj;
    lv_obj_t *icon;
    lv_obj_t *label;
    lv_timer_t *hide_timer;
};

int zmk_widget_timeout_status_init(struct zmk_widget_timeout_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_timeout_status_obj(struct zmk_widget_timeout_status *widget);
void zmk_widget_timeout_status_show_keyboard_name(const char *name);
