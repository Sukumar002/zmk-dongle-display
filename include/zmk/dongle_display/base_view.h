#pragma once

#include <stdbool.h>
#include <stdint.h>

enum zmk_dongle_display_base_view_mode {
    ZMK_DONGLE_DISPLAY_BASE_VIEW_CAT = 0,
    ZMK_DONGLE_DISPLAY_BASE_VIEW_TEXT = 1,
    ZMK_DONGLE_DISPLAY_BASE_VIEW_WPM = 2,
    ZMK_DONGLE_DISPLAY_BASE_VIEW_MODE_COUNT,
};

bool zmk_dongle_display_is_base_layer(uint8_t layer);
enum zmk_dongle_display_base_view_mode zmk_dongle_display_base_view_get_mode(void);
void zmk_dongle_display_base_view_cycle(void);
void zmk_dongle_display_base_view_refresh(void);
