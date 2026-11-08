/*
 * SPDX-License-Identifier: MIT
 *
 * Native LVGL renderer for the 128x32 compact monochrome dashboard.
 *
 * This intentionally uses ordinary LVGL labels and positioned objects rather
 * than a full-screen canvas. It keeps every element independently anchored to
 * the physical 128x32 screen, avoiding canvas color-format conversion and
 * making the layout inspectable in the LVGL object tree.
 */
#include <zephyr/kernel.h>
#include <lvgl.h>
#include "compact_renderer.h"

#define COMPACT_W 128
#define COMPACT_H 32
#define LEFT_X 0
#define RIGHT_TEXT_X 82
#define BATTERY_X 119
#define ROW0_Y 1
#define ROW1_Y 17
#define BATTERY_W 7
#define BATTERY_H 11

static lv_obj_t *root;
static lv_obj_t *usb_label;
static lv_obj_t *usb_state_label;
static lv_obj_t *bt_label;
static lv_obj_t *bt_profile_label;
static lv_obj_t *bt_state_label;
static lv_obj_t *battery_labels[2];
static lv_obj_t *battery_cases[2];
static lv_obj_t *battery_fills[2];

static bool dashboard_visible = true;
static bool usb_ready;
static bool bt_connected;
static bool bt_bonded;
static uint8_t bt_profile;
static bool battery_valid[2];
static uint8_t battery_level[2];
static bool battery_charging[2];

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int x, int y) {
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_unscii_8, LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(label, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(label, 0, LV_PART_MAIN);
    return label;
}

static lv_obj_t *make_battery_case(lv_obj_t *parent, int y) {
    lv_obj_t *case_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(case_obj);
    lv_obj_set_pos(case_obj, BATTERY_X, y);
    lv_obj_set_size(case_obj, BATTERY_W, BATTERY_H);
    lv_obj_set_style_bg_opa(case_obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(case_obj, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(case_obj, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(case_obj, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_all(case_obj, 0, LV_PART_MAIN);

    return case_obj;
}

static void update_battery(uint8_t row) {
    if (row >= 2 || battery_labels[row] == NULL) return;

    if (!battery_valid[row]) {
        lv_label_set_text(battery_labels[row], row == 0 ? "L --%" : "R --%");
        lv_obj_add_flag(battery_cases[row], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_fills[row], LV_OBJ_FLAG_HIDDEN);
        return;
    }

    uint8_t level = MIN(battery_level[row], 100);
    lv_label_set_text_fmt(battery_labels[row], row == 0 ? "L %u%%" : "R %u%%", level);
    lv_obj_clear_flag(battery_cases[row], LV_OBJ_FLAG_HIDDEN);

    if (battery_charging[row]) {
        lv_obj_set_size(battery_fills[row], 3, 7);
        lv_obj_set_pos(battery_fills[row], BATTERY_X + 2,
                       (row == 0 ? ROW0_Y : ROW1_Y) + 3);
    } else {
        uint8_t fill_h = (level * 7U) / 100U;
        if (fill_h == 0) {
            lv_obj_add_flag(battery_fills[row], LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_obj_clear_flag(battery_fills[row], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_size(battery_fills[row], 3, fill_h);
        lv_obj_set_pos(battery_fills[row], BATTERY_X + 2,
                       (row == 0 ? ROW0_Y : ROW1_Y) + 9 - fill_h);
    }
}

static void render(void) {
    if (root == NULL) return;

    if (!dashboard_visible) {
        lv_obj_add_flag(root, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(root, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(usb_state_label, usb_ready ? "+" : "-");
    lv_label_set_text(bt_profile_label,
                      bt_profile < 5 ? (const char *[]){"1", "2", "3", "4", "5"}[bt_profile] : "?");
    lv_label_set_text(bt_state_label, !bt_bonded ? "o" : (bt_connected ? "+" : "-"));

    update_battery(0);
    update_battery(1);
}

int zmk_compact_renderer_init(lv_obj_t *parent) {
    root = lv_obj_create(parent);
    if (root == NULL) return -ENOMEM;

    lv_obj_remove_style_all(root);
    lv_obj_set_pos(root, 0, 0);
    lv_obj_set_size(root, COMPACT_W, COMPACT_H);
    lv_obj_set_layout(root, LV_LAYOUT_NONE);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(root, 0, LV_PART_MAIN);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    /* Upper-left USB status and lower-left Bluetooth/profile status. */
    usb_label = make_label(root, "USB", LEFT_X, ROW0_Y);
    usb_state_label = make_label(root, "-", 23, ROW0_Y);
    bt_label = make_label(root, "BT", LEFT_X, ROW1_Y);
    bt_profile_label = make_label(root, "1", 17, ROW1_Y);
    bt_state_label = make_label(root, "-", 23, ROW1_Y);

    /* Two vertically stacked split-half battery readings at the right edge. */
    for (uint8_t row = 0; row < 2; row++) {
        int y = row == 0 ? ROW0_Y : ROW1_Y;
        battery_labels[row] = make_label(root, row == 0 ? "L --%" : "R --%",
                                         RIGHT_TEXT_X, y);
        battery_cases[row] = make_battery_case(root, y);
        battery_fills[row] = lv_obj_create(root);
        lv_obj_remove_style_all(battery_fills[row]);
        lv_obj_set_style_bg_color(battery_fills[row], lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(battery_fills[row], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_size(battery_fills[row], 3, 1);
        lv_obj_set_pos(battery_fills[row], BATTERY_X + 2, y + 9);
        lv_obj_add_flag(battery_fills[row], LV_OBJ_FLAG_HIDDEN);
    }

    render();
    return 0;
}

void zmk_compact_renderer_set_output(bool usb, bool bt, bool bonded, uint8_t profile) {
    usb_ready = usb;
    bt_connected = bt;
    bt_bonded = bonded;
    bt_profile = profile;
    render();
}

void zmk_compact_renderer_set_battery(uint8_t half, uint8_t level, bool charging, bool present) {
    if (half >= 2) return;
    battery_valid[half] = present && level > 0;
    battery_level[half] = level;
    battery_charging[half] = charging;
    render();
}

void zmk_compact_renderer_set_visible(bool visible) {
    dashboard_visible = visible;
    render();
}
