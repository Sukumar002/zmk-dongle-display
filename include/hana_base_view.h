#pragma once

#include <stdbool.h>
#include <stdint.h>

enum hana_base_layer {
    HANA_BASE_LAYER_HANA = 0,
    HANA_BASE_LAYER_CHOCOFI = 1,
    HANA_BASE_LAYER_COUNT,
};

bool hana_base_view_is_base_layer(uint8_t layer);

enum hana_base_view_mode {
    HANA_BASE_VIEW_CAT = 0,
    HANA_BASE_VIEW_TEXT = 1,
    HANA_BASE_VIEW_WPM = 2,
    HANA_BASE_VIEW_MODE_COUNT,
};

enum hana_base_view_mode hana_base_view_get_mode(void);
void hana_base_view_cycle(void);
