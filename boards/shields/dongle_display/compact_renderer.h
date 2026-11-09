/*
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <lvgl.h>
#include <zephyr/types.h>

int zmk_compact_renderer_init(lv_obj_t *parent);
void zmk_compact_renderer_set_output(bool usb_active, bool usb_ready,
                                     bool bt_connected, bool bt_bonded,
                                     uint8_t profile);
void zmk_compact_renderer_set_battery(uint8_t half, uint8_t level,
                                      bool charging, bool present);
void zmk_compact_renderer_set_visible(bool visible);
void zmk_compact_renderer_set_modifier_view(bool enabled);
void zmk_compact_renderer_set_modifiers(uint8_t modifiers, bool windows_mode);
void zmk_compact_renderer_set_layer(uint8_t index, const char *name);
void zmk_compact_renderer_set_locks(uint8_t indicators, bool windows_mode);
