/*
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>
#include <zephyr/types.h>

int zmk_compact_renderer_init(lv_obj_t *parent);
void zmk_compact_renderer_set_output(bool usb, bool bt, bool bonded, uint8_t profile);
void zmk_compact_renderer_set_battery(uint8_t half, uint8_t level, bool charging, bool present);
void zmk_compact_renderer_set_visible(bool visible);
