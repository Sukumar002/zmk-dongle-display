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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <hana_base_view.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define LAYOUT_SHIFT_FLASH_MS 700
#define LAYOUT_SHIFT_FLASH_REFRESH_MS 30

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
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS)
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
static struct zmk_widget_wpm_status wpm_status_widget;
#endif
static lv_obj_t *screen_root;
static lv_obj_t *layout_shift_flash_obj;
static lv_obj_t *layout_shift_flash_labels[4];
static lv_timer_t *layout_shift_flash_timer;
static int64_t layout_shift_flash_deadline;
static bool timeout_overlay_active;
lv_style_t global_style;

void zmk_display_compact_set_modifier_view(bool enabled);

void zmk_display_timeout_overlay_set(bool active) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    if (timeout_overlay_active == active) return;
    timeout_overlay_active = active;

    if (active) {
        if (output_status_widget.obj)
            lv_obj_add_flag(zmk_widget_output_status_obj(&output_status_widget), LV_OBJ_FLAG_HIDDEN);
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
        if (dongle_battery_status_widget.obj)
            lv_obj_add_flag(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
        if (layer_status_widget.obj) {
            zmk_widget_layer_status_set_overlay_active(true);
            lv_obj_add_flag(zmk_widget_layer_status_obj(&layer_status_widget), LV_OBJ_FLAG_HIDDEN);
        }
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
        if (modifiers_widget.obj)
            lv_obj_add_flag(zmk_widget_modifiers_obj(&modifiers_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
        if (bongo_cat_widget.obj)
            lv_obj_add_flag(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
        if (wpm_status_widget.obj)
            lv_obj_add_flag(zmk_widget_wpm_status_obj(&wpm_status_widget), LV_OBJ_FLAG_HIDDEN);
#endif
    } else {
        if (output_status_widget.obj)
            lv_obj_clear_flag(zmk_widget_output_status_obj(&output_status_widget), LV_OBJ_FLAG_HIDDEN);
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
        if (dongle_battery_status_widget.obj)
            lv_obj_clear_flag(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
        zmk_widget_layer_status_set_overlay_active(false);
        zmk_widget_layer_status_refresh();
#endif
        zmk_display_compact_set_modifier_view(false);
    }
#else
    ARG_UNUSED(active);
#endif
}

static void compact_layout_shift_flash_timer_cb(lv_timer_t *timer) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    if (k_uptime_get() < layout_shift_flash_deadline) {
        if (layout_shift_flash_obj != NULL) lv_obj_move_foreground(layout_shift_flash_obj);
        return;
    }

    lv_timer_pause(timer);
    if (layout_shift_flash_obj != NULL) lv_obj_add_flag(layout_shift_flash_obj, LV_OBJ_FLAG_HIDDEN);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_set_overlay_active(false);
    zmk_widget_layer_status_refresh();
#endif
    zmk_display_compact_set_modifier_view(hana_base_view_is_compact_modifier_view());
#else
    ARG_UNUSED(timer);
#endif
}

static void compact_layout_shift_flash_create(void) {
    if (layout_shift_flash_obj != NULL || screen_root == NULL) return;

    layout_shift_flash_obj = lv_obj_create(screen_root);
    lv_obj_set_size(layout_shift_flash_obj, 128, 32);
    lv_obj_set_style_bg_color(layout_shift_flash_obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(layout_shift_flash_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(layout_shift_flash_obj, 0, 0);
    lv_obj_set_style_pad_all(layout_shift_flash_obj, 0, 0);

    for (int i = 0; i < 4; i++) {
        lv_obj_t *cell = lv_obj_create(layout_shift_flash_obj);
        lv_obj_set_size(cell, 31, 32);
        lv_obj_set_style_radius(cell, 0, 0);
        lv_obj_set_style_border_width(cell, 1, 0);
        lv_obj_set_style_border_color(cell, lv_color_white(), 0);
        lv_obj_set_style_bg_color(cell, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(cell, 0, 0);
        lv_obj_set_pos(cell, i * 32, 0);

        layout_shift_flash_labels[i] = lv_label_create(cell);
        lv_obj_center(layout_shift_flash_labels[i]);
        lv_obj_set_style_text_color(layout_shift_flash_labels[i], lv_color_white(), 0);
    }
    lv_obj_add_flag(layout_shift_flash_obj, LV_OBJ_FLAG_HIDDEN);
}

void hana_base_view_flash_layout_shift(bool windows_mode) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    if (screen_root == NULL) return;

    compact_layout_shift_flash_create();
    static const char *const mac_names[4] = { "CTRL", "OPT", "CMD", "SHIFT" };
    static const char *const windows_names[4] = { "CTRL", "WIN", "ALT", "SHIFT" };
    const char *const *names = windows_mode ? windows_names : mac_names;
    for (int i = 0; i < 4; i++) lv_label_set_text(layout_shift_flash_labels[i], names[i]);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_set_overlay_active(true);
#endif
    lv_obj_clear_flag(layout_shift_flash_obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(layout_shift_flash_obj);
    layout_shift_flash_deadline = k_uptime_get() + LAYOUT_SHIFT_FLASH_MS;

    if (layout_shift_flash_timer == NULL) {
        layout_shift_flash_timer =
            lv_timer_create(compact_layout_shift_flash_timer_cb,
                            LAYOUT_SHIFT_FLASH_REFRESH_MS, NULL);
        lv_timer_pause(layout_shift_flash_timer);
    }
    lv_timer_set_period(layout_shift_flash_timer, LAYOUT_SHIFT_FLASH_REFRESH_MS);
    lv_timer_reset(layout_shift_flash_timer);
    lv_timer_resume(layout_shift_flash_timer);
#else
    ARG_UNUSED(windows_mode);
#endif
}

/* The 128x32 base view always keeps the full-size connection and battery
 * widgets visible. Modifier-view cycling belongs to the taller dashboard. */
void zmk_display_compact_set_modifier_view(bool enabled) {
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    ARG_UNUSED(enabled);
    if (output_status_widget.obj) {
        if (timeout_overlay_active)
            lv_obj_add_flag(zmk_widget_output_status_obj(&output_status_widget), LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_clear_flag(zmk_widget_output_status_obj(&output_status_widget), LV_OBJ_FLAG_HIDDEN);
    }
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    if (dongle_battery_status_widget.obj) {
        if (timeout_overlay_active)
            lv_obj_add_flag(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_clear_flag(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_OBJ_FLAG_HIDDEN);
    }
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    if (modifiers_widget.obj)
        lv_obj_add_flag(zmk_widget_modifiers_obj(&modifiers_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#else
    ARG_UNUSED(enabled);
#endif
}

lv_obj_t *zmk_display_status_screen() {
    screen_root = lv_obj_create(NULL);
    lv_style_init(&global_style);
    lv_style_set_bg_color(&global_style, lv_color_white());
    lv_style_set_bg_opa(&global_style, LV_OPA_COVER);
    lv_style_set_text_color(&global_style, lv_color_black());
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen_root, &global_style, LV_PART_MAIN);

    zmk_widget_output_status_init(&output_status_widget, screen_root);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget), LV_ALIGN_TOP_LEFT, 0, 0);
    zmk_widget_timeout_status_init(&timeout_status_widget, screen_root);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_status_init(&wpm_status_widget, screen_root);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    lv_obj_add_flag(zmk_widget_wpm_status_obj(&wpm_status_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen_root);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    lv_obj_add_flag(zmk_widget_bongo_cat_obj(&bongo_cat_widget), LV_OBJ_FLAG_HIDDEN);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    lv_obj_align(zmk_widget_wpm_status_obj(&wpm_status_widget), LV_ALIGN_BOTTOM_RIGHT, 0, 0);
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_MODIFIERS)
    zmk_widget_modifiers_init(&modifiers_widget, screen_root);
    lv_obj_add_flag(zmk_widget_modifiers_obj(&modifiers_widget), LV_OBJ_FLAG_HIDDEN);
#if IS_ENABLED(CONFIG_ZMK_HID_INDICATORS) && !IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, screen_root);
    lv_obj_align_to(zmk_widget_hid_indicators_obj(&hid_indicators_widget), zmk_widget_modifiers_obj(&modifiers_widget), LV_ALIGN_OUT_TOP_LEFT, 0, -2);
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_init(&layer_status_widget, screen_root);
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    lv_obj_align(zmk_widget_layer_status_obj(&layer_status_widget), LV_ALIGN_CENTER, 0, 0);
    zmk_widget_layer_status_set_base_text_enabled(false);
#endif
#endif
#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen_root);
    lv_obj_align(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget), LV_ALIGN_TOP_RIGHT, 0, 0);
#endif
#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_COMPACT_MODE)
    zmk_display_compact_set_modifier_view(false);
#endif
    return screen_root;
}
