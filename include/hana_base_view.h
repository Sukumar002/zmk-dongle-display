#pragma once

#include <stdint.h>

enum hana_base_view_mode {
    HANA_BASE_VIEW_CAT_WPM = 0,
    HANA_BASE_VIEW_TEXT_WPM = 1,
    HANA_BASE_VIEW_CAT_ONLY = 2,
    HANA_BASE_VIEW_TEXT_ONLY = 3,
    HANA_BASE_VIEW_MODE_COUNT,
};

enum hana_base_view_mode hana_base_view_get_mode(void);
void hana_base_view_cycle(void);
