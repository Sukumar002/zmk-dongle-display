/*
 * SPDX-License-Identifier: MIT
 *
 * Fixed-coordinate renderer for compact monochrome status displays.
 * The dashboard is drawn into one 128x32 canvas; no child widgets or
 * LVGL layout containers participate in normal status rendering.
 */
#include <zephyr/kernel.h>
#include <lvgl.h>
#include "compact_renderer.h"

#define COMPACT_W 128
#define COMPACT_H 32
#define GLYPH_W 5
#define GLYPH_H 7
#define GLYPH_ADVANCE 6
#define TEXT_COLOR lv_color_black()

static lv_obj_t *canvas;
LV_ATTRIBUTE_MEM_ALIGN static uint8_t canvas_buffer[LV_CANVAS_BUF_SIZE(COMPACT_W, COMPACT_H,
        LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)];

static bool dashboard_visible = true;
static bool usb_ready;
static bool bt_connected;
static bool bt_bonded;
static uint8_t bt_profile;
static bool battery_valid[2];
static uint8_t battery_level[2];
static bool battery_charging[2];
static uint8_t active_layer;
static char active_layer_name[32] = "BASE";

static const uint8_t *glyph(char c) {
    static const uint8_t blank[7] = {0};
    /* Five columns, seven rows; bit 0 is the top pixel. */
    static const uint8_t digits[10][7] = {
        {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
        {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
        {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
        {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
        {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12}
    };
    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
        {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
        {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c >= 'a' && c <= 'z') return letters[c - 'a'];
    static const uint8_t percent[7] = {17,2,4,8,17,0,0};
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    static const uint8_t colon[7] = {0,4,4,0,4,4,0};
    static const uint8_t dot[7] = {0,0,0,0,0,4,4};
    if (c == '%') return percent;
    if (c == '-') return dash;
    if (c == ':') return colon;
    if (c == '.') return dot;
    return blank;
}

static void px(int x, int y) {
    if (canvas && x >= 0 && x < COMPACT_W && y >= 0 && y < COMPACT_H)
        lv_canvas_set_px(canvas, x, y, TEXT_COLOR, LV_OPA_COVER);
}

static void hline(int x, int y, int width) {
    for (int i = 0; i < width; i++) px(x + i, y);
}

static void vline(int x, int y, int height) {
    for (int i = 0; i < height; i++) px(x, y + i);
}

static void draw_char(int x, int y, char c) {
    const uint8_t *rows = glyph(c);
    for (int row = 0; row < GLYPH_H; row++) {
        for (int col = 0; col < GLYPH_W; col++) {
            if (rows[row] & (1U << (GLYPH_W - 1 - col))) px(x + col, y + row);
        }
    }
}

static int text_width(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n ? n * GLYPH_ADVANCE - 1 : 0;
}

static void draw_text(int x, int y, const char *s) {
    if (!s) return;
    while (*s) {
        draw_char(x, y, *s++);
        x += GLYPH_ADVANCE;
    }
}

static void draw_centered(const char *s) {
    int width = text_width(s);
    int x = (COMPACT_W - width) / 2;
    int y = (COMPACT_H - GLYPH_H) / 2;
    draw_text(x, y, s);
}

static void draw_usb_icon(void) {
    /* USB trident: stem, upper branch, lower branch and connector tips. */
    vline(5, 1, 12);
    hline(2, 4, 7);
    px(2, 3); px(3, 2); px(4, 1);
    px(1, 5); px(2, 6); px(3, 5);
    px(4, 11); px(5, 12); px(6, 11);
    hline(5, 1, 1);
    px(5, 0);
}

static void draw_bt_icon(void) {
    /* Compact Bluetooth rune, 9x14. */
    /* Draw the rune explicitly with diagonals and central stem. */
    vline(4, 18, 13);
    px(4,18); px(5,19); px(6,20); px(7,21); px(6,22); px(5,23);
    px(4,24); px(5,25); px(6,26); px(7,27); px(6,28); px(5,29);
    px(4,30); px(3,29); px(2,28); px(1,27);
    px(3,25); px(2,24); px(1,23); px(2,22); px(3,21);
    px(3,19); px(4,18);
}

static void draw_status_mark(int x, int y, bool good, bool open) {
    if (open) {
        hline(x + 1, y, 3); px(x, y + 1); px(x + 4, y + 1);
        px(x, y + 2); px(x + 4, y + 2); hline(x + 1, y + 3, 3);
    } else if (good) {
        px(x, y + 2); px(x + 1, y + 3); px(x + 2, y + 2);
        px(x + 3, y + 1); px(x + 4, y);
    } else {
        px(x, y); px(x + 4, y); px(x + 1, y + 1); px(x + 3, y + 1);
        px(x + 2, y + 2); px(x + 1, y + 3); px(x + 3, y + 3);
        px(x, y + 4); px(x + 4, y + 4);
    }
}

static void draw_battery_row(int row, char side) {
    const int y = row == 0 ? 3 : 19;
    char value[5] = { side, ' ', '-', '-', 0 };
    if (battery_valid[row]) {
        int level = battery_level[row] > 100 ? 100 : battery_level[row];
        if (level == 100) {
            value[1] = '1'; value[2] = '0'; value[3] = '0';
        } else {
            value[2] = (char)('0' + level / 10);
            value[3] = (char)('0' + level % 10);
        }
    }
    /* Fixed four-character fields: L 00 / R 00, or L100 / R100. */
    draw_text(99, y + 1, value);
}

static void draw_layer_panel(void) {
    const int x = 88, y = 3, width = 39, height = 26;
    hline(x, y, width); hline(x, y + height - 1, width);
    vline(x, y, height); vline(x + width - 1, y, height);

    char label[7];
    size_t n = 0;
    while (active_layer_name[n] && n < sizeof(label) - 1) {
        label[n] = active_layer_name[n];
        n++;
    }
    label[n] = '\0';
    int text_px = text_width(label);
    if (text_px > width - 6) {
        label[6] = '\0';
        text_px = text_width(label);
    }
    draw_text(x + (width - text_px) / 2, 12, label);
}

static void render(void) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    if (!dashboard_visible) return;

    draw_usb_icon();
    draw_status_mark(13, 5, usb_ready, false);
    draw_bt_icon();
    draw_text(13, 22, bt_profile < 5 ? (const char *[]){"1","2","3","4","5"}[bt_profile] : "?");
    draw_status_mark(21, 22, bt_connected, !bt_bonded);

    if (active_layer == 0) {
        draw_battery_row(0, 'L');
        draw_battery_row(1, 'R');
    } else {
        draw_layer_panel();
    }
}

int zmk_compact_renderer_init(lv_obj_t *parent) {
    canvas = lv_canvas_create(parent);
    if (!canvas) return -ENOMEM;
    lv_canvas_set_buffer(canvas, canvas_buffer, COMPACT_W, COMPACT_H, LV_COLOR_FORMAT_L8);
    lv_obj_set_pos(canvas, 0, 0);
    lv_obj_set_size(canvas, COMPACT_W, COMPACT_H);
    lv_obj_remove_style_all(canvas);
    lv_obj_set_style_bg_opa(canvas, LV_OPA_TRANSP, LV_PART_MAIN);
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

void zmk_compact_renderer_set_layer(uint8_t layer, const char *name) {
    active_layer = layer;
    if (name == NULL || name[0] == '\0') {
        active_layer_name[0] = '\0';
    } else {
        size_t n = 0;
        while (name[n] && n < sizeof(active_layer_name) - 1) {
            active_layer_name[n] = name[n];
            n++;
        }
        active_layer_name[n] = '\0';
    }
    render();
}

void zmk_compact_renderer_set_visible(bool visible) {
    dashboard_visible = visible;
    render();
}
