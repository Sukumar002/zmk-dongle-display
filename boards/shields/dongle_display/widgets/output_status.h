/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
 #pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_output_status {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *usb;
    lv_obj_t *usb_hid_status;
    lv_obj_t *bt;
    lv_obj_t *bt_number;
    lv_obj_t *bt_status;
    lv_obj_t *selection_line;
};

int zmk_widget_output_status_init(struct zmk_widget_output_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_output_status_obj(struct zmk_widget_output_status *widget);
void zmk_widget_output_status_set_hidden(struct zmk_widget_output_status *widget, bool hidden);