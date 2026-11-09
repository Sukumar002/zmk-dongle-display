/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>
#include <zephyr/kernel.h>
struct zmk_widget_compact_modifiers {
    sys_snode_t node;
    lv_obj_t *obj;
};
int zmk_widget_compact_modifiers_init(struct zmk_widget_compact_modifiers *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_compact_modifiers_obj(struct zmk_widget_compact_modifiers *widget);
