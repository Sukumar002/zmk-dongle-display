/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/modifiers.h"
#include "widgets/bongo_cat.h"
#include "widgets/layer_status.h"
#include "widgets/output_status.h"
#include "widgets/hid_indicators.h"
#include "widgets/wpm_status.h"
#include "widgets/timeout_status.h"
#include <zmk/universal_dongle_oled_status.h>
#include <hana_base_view.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_timeout_status timeout_status_widget;

#if IS_ENABLED(CONFIG_ZMK_BATTERY) && !IS_ENABLED(CONFIG_ZMK_UNIVERSAL_DONGLE_CENTRAL_IDENTITY)
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
static struct zmk_widget_layer_status layer_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
static struct zmk_widget_modifiers modifiers_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif

static bool compact_widgets_ready;

lv_style_t global_style;

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
/*
 * The 128x32 screen has two explicit views, controlled by the existing
 * base_view_tog key:
 *
 * Dashboard: connection state on the left, battery stack on the right.
 * Modifiers: the four OS-family modifier symbols across the top.
 *
 * Active HID locks remain independently visible as a centered badge at the
 * bottom. Timeout feedback is a full-screen foreground overlay.
 */
void zmk_dongle_display_set_compact_view(bool modifiers_view) {
    if (!compact_widgets_ready) {
        return;
    }

    if (modifiers_view) {
        lv_obj_add_flag(zmk_widget_output_status_obj(&output_status_widget),
                        LV_OBJ_FLAG_HIDDEN);
#if IS_ENABLED(CONFIG_ZMK_BATTERY) && !IS_ENABLED(CONFIG_ZMK_UNIVERSAL_DONGLE_CENTRAL_IDENTITY)
        lv_obj_add_flag(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                        LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
        lv_obj_clear_flag(zmk_widget_modifiers_obj(&modifiers_widget),
                          LV_OBJ_FLAG_HIDDEN);
#endif
    } else {
        lv_obj_clear_flag(zmk_widget_output_status_obj(&output_status_widget),
                          LV_OBJ_FLAG_HIDDEN);
#if IS_ENABLED(CONFIG_ZMK_BATTERY) && !IS_ENABLED(CONFIG_ZMK_UNIVERSAL_DONGLE_CENTRAL_IDENTITY)
        lv_obj_clear_flag(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                          LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
        lv_obj_add_flag(zmk_widget_modifiers_obj(&modifiers_widget),
                        LV_OBJ_FLAG_HIDDEN);
#endif
    }
}
#endif

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget),
                 LV_ALIGN_TOP_LEFT, 0, 0);

    zmk_widget_timeout_status_init(&timeout_status_widget, screen);
    lv_obj_align(zmk_widget_timeout_status_obj(&timeout_status_widget),
                 LV_ALIGN_CENTER, 0, 0);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_128X32)
#if IS_ENABLED(CONFIG_ZMK_BATTERY) && !IS_ENABLED(CONFIG_ZMK_UNIVERSAL_DONGLE_CENTRAL_IDENTITY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                 LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget),
                 LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(zmk_widget_modifiers_obj(&modifiers_widget), LV_OBJ_FLAG_HIDDEN);
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
    lv_obj_align(zmk_widget_hid_indicators_obj(&hid_indicators_widget),
                 LV_ALIGN_BOTTOM_MID, 0, 0);
#endif

    zmk_widget_layer_status_compact_init(screen);

    compact_widgets_ready = true;
    zmk_dongle_display_set_compact_view(hana_base_view_is_compact_modifiers());
    return screen;
#else
#if IS_ENABLED(CONFIG_ZMK_UNIVERSAL_DONGLE_CENTRAL_IDENTITY)
    zmk_universal_oled_status_init(screen, zmk_widget_timeout_status_obj(&timeout_status_widget));
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    lv_obj_align(zmk_widget_wpm_status_obj(&wpm_status_widget),
                 LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_CENTERED_LOCKS)
    lv_obj_align(zmk_widget_hid_indicators_obj(&hid_indicators_widget),
                 LV_ALIGN_BOTTOM_MID, 0, -1);
#elif IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    lv_obj_align_to(zmk_widget_hid_indicators_obj(&hid_indicators_widget),
                    zmk_widget_modifiers_obj(&modifiers_widget),
                    LV_ALIGN_OUT_TOP_LEFT, 0, -2);
#endif
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_init(&layer_status_widget, screen);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_BATTERY) && !IS_ENABLED(CONFIG_ZMK_UNIVERSAL_DONGLE_CENTRAL_IDENTITY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif

    return screen;
#endif
}
