/*
 * Copyright (c) 2024 The ZMK Contributors
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
#include "compact_renderer.h"

#include <zephyr/logging/log.h>
#include <hana_base_view.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_output_status output_status_widget;
static struct zmk_widget_timeout_status timeout_status_widget;
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
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

static lv_obj_t *screen_root;
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
static bool timeout_overlay_active;
#endif
lv_style_t global_style;

void zmk_display_compact_set_modifier_view(bool enabled) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    zmk_compact_renderer_set_modifier_view(enabled);
#else
    ARG_UNUSED(enabled);
#endif
}

/* Full-screen OS flash feedback is intentionally disabled on the 128x32
 * layout so the connection and battery/layer regions remain continuously live. */
void hana_base_view_flash_layout_shift(bool windows_mode) {
    ARG_UNUSED(windows_mode);
}

void zmk_display_timeout_overlay_set(bool active) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    if (timeout_overlay_active == active) return;
    timeout_overlay_active = active;
    zmk_compact_renderer_set_visible(!active);
#else
    ARG_UNUSED(active);
#endif
}

lv_obj_t *zmk_display_status_screen() {
    screen_root = lv_obj_create(NULL);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    /* Use the physical panel bounds; do not inherit theme padding/layout. */
    lv_obj_set_layout(screen_root, LV_LAYOUT_NONE);
    lv_obj_remove_style_all(screen_root);
    lv_obj_set_style_pad_all(screen_root, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen_root, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_root, LV_OPA_COVER, LV_PART_MAIN);
#endif

    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen_root, &global_style, LV_PART_MAIN);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    /* One LVGL canvas owns both dashboard regions and the center lock band. */
    zmk_compact_renderer_init(screen_root);
#endif

    zmk_widget_output_status_init(&output_status_widget, screen_root);
#if !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget),
                 LV_ALIGN_TOP_LEFT, 0, 0);
#endif
    zmk_widget_timeout_status_init(&timeout_status_widget, screen_root);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    lv_obj_align(zmk_widget_timeout_status_obj(&timeout_status_widget),
                 LV_ALIGN_CENTER, 0, 0);
#else
    lv_obj_align(zmk_widget_timeout_status_obj(&timeout_status_widget),
                 LV_ALIGN_LEFT_MID, 0, 0);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen_root);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_init(&layer_status_widget, screen_root);
#endif
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen_root);
#endif
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen_root);
#endif
    zmk_display_compact_set_modifier_view(hana_base_view_is_compact_modifier_view());
#else
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen_root);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen_root);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    lv_obj_align(zmk_widget_wpm_status_obj(&wpm_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen_root);
    lv_obj_align(zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_BOTTOM_LEFT, 0, 0);
#endif
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen_root);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    lv_obj_align_to(zmk_widget_hid_indicators_obj(&hid_indicators_widget),
                    zmk_widget_modifiers_obj(&modifiers_widget),
                    LV_ALIGN_OUT_TOP_LEFT, 0, -2);
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_init(&layer_status_widget, screen_root);
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen_root);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                 LV_ALIGN_TOP_RIGHT, 0, 0);
#endif
#endif
    return screen_root;
}
