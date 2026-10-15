/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <stdbool.h>

struct zmk_widget_layer_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *label;
    lv_obj_t *label_bold;
    uint8_t layer;
};

int zmk_widget_layer_status_init(struct zmk_widget_layer_status *widget, lv_obj_t *parent);
void zmk_widget_layer_status_set_base_text_enabled(bool enabled);
lv_obj_t *zmk_widget_layer_status_obj(struct zmk_widget_layer_status *widget);
