/*
 * SPDX-License-Identifier: MIT
 *
 * Compact 128x32 status dashboard rendered on an LVGL canvas. ZMK's normal
 * display subsystem owns the OLED; this renderer only paints the dashboard.
 */
#include <zephyr/kernel.h>
#include <string.h>
#include <lvgl.h>
#include "compact_renderer.h"

#define COMPACT_W 128
#define COMPACT_H 32
#define GLYPH_W 5
#define GLYPH_H 7
#define GLYPH_ADVANCE 6
#define LEFT_REGION_END 38
#define RIGHT_REGION_START 88
#define LAYER_TEXT_LEFT 92
#define LAYER_TEXT_RIGHT 123
#define TEXT_COLOR lv_color_black()

static lv_obj_t *canvas;
LV_ATTRIBUTE_MEM_ALIGN static uint8_t canvas_buffer[LV_CANVAS_BUF_SIZE(
    COMPACT_W, COMPACT_H, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8),
    LV_DRAW_BUF_STRIDE_ALIGN)];

static bool dashboard_visible = true;
static bool modifier_view;
static bool usb_ready;
static bool usb_active;
static bool bt_connected;
static bool bt_bonded;
static uint8_t bt_profile;
static bool battery_valid[2];
static uint8_t battery_level[2];
static uint8_t active_modifiers;
static bool windows_mode;
static uint8_t lock_state;
static bool lock_windows_mode;
static uint8_t active_layer;
static char active_layer_name[48] = "BASE";
static uint16_t layer_scroll_px;
static lv_timer_t *layer_scroll_timer;

static const uint8_t *glyph(char c) {
    static const uint8_t blank[7] = {0};
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
    static const uint8_t dash[7] = {0,0,0,31,0,0,0};
    if (c >= '0' && c <= '9') return digits[c - '0'];
    if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
    if (c >= 'a' && c <= 'z') return letters[c - 'a'];
    if (c == '-') return dash;
    return blank;
}

static void set_px(int x, int y, lv_color_t color) {
    if (canvas && x >= 0 && x < COMPACT_W && y >= 0 && y < COMPACT_H) {
        lv_canvas_set_px(canvas, x, y, color, LV_OPA_COVER);
    }
}
static void px(int x, int y) { set_px(x, y, TEXT_COLOR); }
static void hline(int x, int y, int width) {
    for (int i = 0; i < width; i++) px(x + i, y);
}
static void vline(int x, int y, int height) {
    for (int i = 0; i < height; i++) px(x, y + i);
}
static void fill_rect(int x, int y, int width, int height, lv_color_t color) {
    for (int yy = y; yy < y + height; yy++)
        for (int xx = x; xx < x + width; xx++) set_px(xx, yy, color);
}
static void outline_rect(int x, int y, int width, int height) {
    hline(x, y, width);
    hline(x, y + height - 1, width);
    vline(x, y, height);
    vline(x + width - 1, y, height);
}
static void draw_char(int x, int y, char c) {
    const uint8_t *rows = glyph(c);
    for (int row = 0; row < GLYPH_H; row++)
        for (int col = 0; col < GLYPH_W; col++)
            if (rows[row] & (1U << (GLYPH_W - 1 - col))) px(x + col, y + row);
}
static int text_width(const char *s) {
    size_t n = s ? strlen(s) : 0;
    return n ? (int)n * GLYPH_ADVANCE - 1 : 0;
}
static void draw_text(int x, int y, const char *s) {
    if (!s) return;
    while (*s) {
        draw_char(x, y, *s++);
        x += GLYPH_ADVANCE;
    }
}
static void draw_text_clipped(int x, int y, const char *s) {
    if (!s) return;
    while (*s) {
        const uint8_t *rows = glyph(*s++);
        for (int row = 0; row < GLYPH_H; row++) {
            for (int col = 0; col < GLYPH_W; col++) {
                int px_x = x + col;
                if (px_x >= LAYER_TEXT_LEFT && px_x <= LAYER_TEXT_RIGHT &&
                    (rows[row] & (1U << (GLYPH_W - 1 - col)))) px(px_x, y + row);
            }
        }
        x += GLYPH_ADVANCE;
    }
}

static void draw_usb_icon(void) {
    vline(5, 1, 12);
    hline(2, 4, 7);
    px(2, 3); px(3, 2); px(4, 1);
    px(1, 5); px(2, 6); px(3, 5);
    px(4, 11); px(5, 12); px(6, 11);
    px(5, 0);
}
static void draw_bt_icon(void) {
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
    const int y = row == 0 ? 2 : 18;
    char value[5] = { side, ' ', '-', '-', 0 };
    if (battery_valid[row]) {
        unsigned int level = MIN(battery_level[row], 100);
        if (level == 100) {
            value[1] = '1'; value[2] = '0'; value[3] = '0';
        } else {
            value[2] = '0' + (level / 10);
            value[3] = '0' + (level % 10);
        }
    }
    draw_text(99, y, value);
}

/* 14x14 Hana modifier artwork; each row is two bytes, MSB first. */
static const uint8_t modifier_control[28] = {
    0x00,0x00,0x00,0x00,0x03,0x00,0x07,0x80,0x0c,0xc0,0x18,0x60,0x30,0x30,
    0x20,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t modifier_shift[28] = {
    0x00,0x00,0x00,0x00,0x03,0x00,0x07,0x80,0x0c,0xc0,0x18,0x60,0x30,0x30,
    0x78,0x78,0x08,0x40,0x08,0x40,0x08,0x40,0x0f,0xc0,0x00,0x00,0x00,0x00
};
static const uint8_t modifier_opt[28] = {
    0x00,0x00,0x00,0x00,0x3c,0xe0,0x3c,0xe0,0x06,0x00,0x06,0x00,0x06,0x00,
    0x03,0x00,0x03,0x00,0x03,0x00,0x01,0xe0,0x01,0xe0,0x00,0x00,0x00,0x00
};
static const uint8_t modifier_cmd[28] = {
    0x00,0x00,0x00,0x00,0x18,0x60,0x24,0x90,0x24,0x90,0x1f,0xe0,0x04,0x80,
    0x04,0x80,0x1f,0xe0,0x24,0x90,0x24,0x90,0x18,0x60,0x00,0x00,0x00,0x00
};
static const uint8_t modifier_alt[28] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x32,0xf8,0x4a,0x20,0x4a,0x20,0x7a,0x20,
    0x4a,0x20,0x4b,0xa0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static const uint8_t modifier_win[28] = {
    0x00,0x00,0x00,0x00,0x00,0xf0,0x0b,0xf0,0x3b,0xf0,0x3b,0xf0,0x3b,0xf0,
    0x00,0x00,0x3b,0xf0,0x3b,0xf0,0x3b,0xf0,0x03,0xf0,0x00,0x30,0x00,0x00
};
static void draw_modifier_icon(int x, int y, const uint8_t *bits, bool active) {
    if (active) fill_rect(x, y, 14, 14, lv_color_black());
    for (int row = 0; row < 14; row++) {
        for (int col = 0; col < 14; col++) {
            bool set = (bits[row * 2 + col / 8] & (0x80U >> (col & 7))) != 0;
            if (set) set_px(x + col, y + row, active ? lv_color_white() : lv_color_black());
        }
    }
}
static void draw_modifier_family(void) {
    const uint8_t masks[4] = {
        MOD_LCTL | MOD_RCTL,
        windows_mode ? (MOD_LGUI | MOD_RGUI) : (MOD_LALT | MOD_RALT),
        windows_mode ? (MOD_LALT | MOD_RALT) : (MOD_LGUI | MOD_RGUI),
        MOD_LSFT | MOD_RSFT,
    };
    const uint8_t *icons[4] = {
        modifier_control,
        windows_mode ? modifier_win : modifier_opt,
        windows_mode ? modifier_alt : modifier_cmd,
        modifier_shift,
    };
    static const int xs[4] = {3, 22, 3, 22};
    static const int ys[4] = {1, 1, 17, 17};
    for (int i = 0; i < 4; i++)
        draw_modifier_icon(xs[i], ys[i], icons[i], (active_modifiers & masks[i]) != 0);
}
static void draw_locks(void) {
    const uint8_t masks[3] = {0x02, 0x01, 0x04};
    const char labels[3] = {'C', 'N', 'S'};
    int count = 0;
    for (int i = 0; i < 3; i++) {
        if ((lock_state & masks[i]) && (i == 0 || lock_windows_mode)) count++;
    }
    if (!count) return;
    const int size = 12, gap = 3;
    const int total = count * size + (count - 1) * gap;
    int x = (COMPACT_W - total) / 2;
    for (int i = 0; i < 3; i++) {
        if (!(lock_state & masks[i]) || (i != 0 && !lock_windows_mode)) continue;
        outline_rect(x, 10, size, size);
        char c[2] = {labels[i], 0};
        draw_text(x + 3, 12, c);
        x += size + gap;
    }
}
static void draw_layer_badge(void) {
    const int x = 88, y = 3, w = 39, h = 26;
    outline_rect(x, y, w, h);
    int width = text_width(active_layer_name);
    if (width <= (LAYER_TEXT_RIGHT - LAYER_TEXT_LEFT + 1)) {
        int tx = x + (w - width) / 2;
        draw_text_clipped(tx, 12, active_layer_name);
    } else {
        int cycle = width + 12;
        int offset = layer_scroll_px % cycle;
        draw_text_clipped(LAYER_TEXT_LEFT - offset, 12, active_layer_name);
        draw_text_clipped(LAYER_TEXT_LEFT + width + 12 - offset, 12, active_layer_name);
    }
}
static void render(void) {
    if (!canvas) return;
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    if (!dashboard_visible) return;

    if (modifier_view) {
        draw_modifier_family();
    } else {
        draw_usb_icon();
        draw_status_mark(17, 5, usb_ready, false);
        draw_bt_icon();
        draw_text(17, 25, bt_profile < 5 ? (const char *[]){"1","2","3","4","5"}[bt_profile] : "?");
        draw_status_mark(25, 21, bt_connected, !bt_bonded);
        if (usb_active) { vline(1, 3, 10); vline(2, 3, 10); }
        else { vline(1, 20, 10); vline(2, 20, 10); }
    }

    if (active_layer == 0) {
        draw_battery_row(0, 'L');
        draw_battery_row(1, 'R');
    } else {
        draw_layer_badge();
    }
    draw_locks();
}
static void layer_scroll_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);
    if (active_layer == 0 || text_width(active_layer_name) <= 32) return;
    layer_scroll_px++;
    if (layer_scroll_px >= text_width(active_layer_name) + 12) layer_scroll_px = 0;
    render();
}
static void update_layer_timer(void) {
    if (!layer_scroll_timer) return;
    if (active_layer != 0 && text_width(active_layer_name) > 32) {
        layer_scroll_px = 0;
        lv_timer_set_period(layer_scroll_timer, 45);
        lv_timer_resume(layer_scroll_timer);
    } else {
        lv_timer_pause(layer_scroll_timer);
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
    layer_scroll_timer = lv_timer_create(layer_scroll_cb, 45, NULL);
    lv_timer_pause(layer_scroll_timer);
    render();
    return 0;
}
void zmk_compact_renderer_set_output(bool usb_active_now, bool usb_is_ready,
                                     bool bt, bool bonded, uint8_t profile) {
    usb_active = usb_active_now;
    usb_ready = usb_is_ready;
    bt_connected = bt;
    bt_bonded = bonded;
    bt_profile = profile;
    render();
}
void zmk_compact_renderer_set_battery(uint8_t half, uint8_t level,
                                      bool charging, bool present) {
    ARG_UNUSED(charging);
    if (half >= 2) return;
    battery_valid[half] = present;
    battery_level[half] = level;
    render();
}
void zmk_compact_renderer_set_visible(bool visible) {
    dashboard_visible = visible;
    render();
}
void zmk_compact_renderer_set_modifier_view(bool enabled) {
    modifier_view = enabled;
    render();
}
void zmk_compact_renderer_set_modifiers(uint8_t modifiers, bool windows) {
    active_modifiers = modifiers;
    windows_mode = windows;
    render();
}
void zmk_compact_renderer_set_layer(uint8_t index, const char *name) {
    active_layer = index;
    if (name && name[0]) {
        snprintk(active_layer_name, sizeof(active_layer_name), "%s", name);
    } else {
        snprintk(active_layer_name, sizeof(active_layer_name), "LAYER %u", index);
    }
    layer_scroll_px = 0;
    update_layer_timer();
    render();
}
void zmk_compact_renderer_set_locks(uint8_t indicators, bool windows) {
    lock_state = indicators;
    lock_windows_mode = windows;
    render();
}
