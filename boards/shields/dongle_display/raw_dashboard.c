/*
 * SPDX-License-Identifier: MIT
 *
 * Hana 128x32 base status screen.
 * The raw renderer is the sole OLED writer in this build; no LVGL widgets
 * or legacy base-view code are initialized.
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/event_manager.h>
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/endpoints.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/hid.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/events/runtime_timeout_changed.h>
#include <dt-bindings/zmk/hid_indicators.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/modifiers.h>

LOG_MODULE_REGISTER(hana_128x32_dashboard, LOG_LEVEL_INF);

#define OLED_W 128
#define OLED_H 32
#define FB_SIZE (OLED_W * OLED_H / 8)

static const struct device *const oled = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static uint8_t framebuffer[FB_SIZE];
static uint8_t half_level[2] = { 0xff, 0xff };
static bool bt_connected;
static bool bt_bonded;
static uint8_t bt_profile;
static enum zmk_transport active_transport = ZMK_TRANSPORT_NONE;
static zmk_hid_indicators_t lock_indicators;

/* Base view 0 is the original dashboard; 1 is the modifier family. */
static uint8_t base_view_mode;
static bool layout_windows_mode;
static bool layout_flash_active;
static uint8_t active_layer;
static char active_layer_name[64] = "BASE";
static uint16_t layer_scroll_px;
static struct k_work_delayable layer_scroll_work;
static struct k_work_delayable layout_probe_work;
static struct k_work_delayable layout_flash_work;
static bool timeout_feedback_active;
static enum zmk_runtime_timeout_kind timeout_feedback_kind;
static uint32_t timeout_feedback_ms;
static struct k_work_delayable timeout_feedback_work;
static bool raw_manual_display_off;

#if DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt_shared))
static const struct device *const hana_layout_shift_map =
    DEVICE_DT_GET(DT_NODELABEL(layout_shift_map_gui_alt_shared));
struct hana_layout_shift_map_data_view {
    zmk_mod_flags_t modifier_map[8];
    bool active;
};
#endif

static bool read_layout_windows_mode(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt_shared))
    if (!device_is_ready(hana_layout_shift_map)) return false;
    const struct hana_layout_shift_map_data_view *data = hana_layout_shift_map->data;
    return data->active;
#else
    return false;
#endif
}

static int dashboard_render(void);
static void raw_apply_display_power(void)
{
    if (!device_is_ready(oled)) {
        return;
    }

    if (raw_manual_display_off || zmk_activity_get_state() != ZMK_ACTIVITY_ACTIVE) {
        (void)display_blanking_on(oled);
    } else {
        (void)display_blanking_off(oled);
    }
}

/* Mirrors ZMK's LVGL display behavior: a new active session clears manual-off. */
void zmk_raw_display_toggle_manual_power(void)
{
    raw_manual_display_off = !raw_manual_display_off;
    raw_apply_display_power();
}


/* The existing &base_view_tog behavior calls this entry point. */
void hana_base_view_cycle(void)
{
    base_view_mode = (base_view_mode + 1) % 2;
#if IS_ENABLED(CONFIG_SETTINGS)
    (void)settings_save_one("hana_base_view_32/modifier_view",
                            &base_view_mode, sizeof(base_view_mode));
#endif
    (void)dashboard_render();
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int hana_raw_base_view_settings_set(const char *name, size_t len,
                                           settings_read_cb read_cb, void *cb_arg)
{
    if (strcmp(name, "modifier_view") != 0 || len != sizeof(base_view_mode)) return -ENOENT;
    int rc = read_cb(cb_arg, &base_view_mode, sizeof(base_view_mode));
    if (rc < 0) return rc;
    if (base_view_mode > 1) base_view_mode = 0;
    return 0;
}
SETTINGS_STATIC_HANDLER_DEFINE(hana_base_view_32, "hana_base_view_32", NULL,
                               hana_raw_base_view_settings_set, NULL, NULL);
#endif

/* 5x7 column font. Bit zero is the top row. */
static const uint8_t font[][5] = {
 [0]={0x3e,0x51,0x49,0x45,0x3e}, [1]={0x00,0x42,0x7f,0x40,0x00},
 [2]={0x42,0x61,0x51,0x49,0x46}, [3]={0x21,0x41,0x45,0x4b,0x31},
 [4]={0x18,0x14,0x12,0x7f,0x10}, [5]={0x27,0x45,0x45,0x45,0x39},
 [6]={0x3c,0x4a,0x49,0x49,0x30}, [7]={0x01,0x71,0x09,0x05,0x03},
 [8]={0x36,0x49,0x49,0x49,0x36}, [9]={0x06,0x49,0x49,0x29,0x1e},
 ['A'-32]={0x7e,0x11,0x11,0x11,0x7e}, ['B'-32]={0x7f,0x49,0x49,0x49,0x36},
 ['C'-32]={0x3e,0x41,0x41,0x41,0x22}, ['D'-32]={0x7f,0x41,0x41,0x22,0x1c},
 ['E'-32]={0x7f,0x49,0x49,0x49,0x41}, ['F'-32]={0x7f,0x09,0x09,0x09,0x01},
 ['G'-32]={0x3e,0x41,0x49,0x49,0x7a}, ['H'-32]={0x7f,0x08,0x08,0x08,0x7f},
 ['I'-32]={0x00,0x41,0x7f,0x41,0x00}, ['J'-32]={0x20,0x40,0x41,0x3f,0x01},
 ['K'-32]={0x7f,0x08,0x14,0x22,0x41}, ['L'-32]={0x7f,0x40,0x40,0x40,0x40},
 ['M'-32]={0x7f,0x02,0x0c,0x02,0x7f}, ['N'-32]={0x7f,0x04,0x08,0x10,0x7f},
 ['O'-32]={0x3e,0x41,0x41,0x41,0x3e}, ['P'-32]={0x7f,0x09,0x09,0x09,0x06},
 ['Q'-32]={0x3e,0x41,0x51,0x21,0x5e}, ['R'-32]={0x7f,0x09,0x19,0x29,0x46},
 ['S'-32]={0x26,0x49,0x49,0x49,0x32}, ['T'-32]={0x01,0x01,0x7f,0x01,0x01},
 ['U'-32]={0x3f,0x40,0x40,0x40,0x3f}, ['V'-32]={0x1f,0x20,0x40,0x20,0x1f},
 ['W'-32]={0x3f,0x40,0x38,0x40,0x3f}, ['X'-32]={0x63,0x14,0x08,0x14,0x63},
 ['Y'-32]={0x07,0x08,0x70,0x08,0x07}, ['Z'-32]={0x61,0x51,0x49,0x45,0x43},
 ['%'-32]={0x63,0x13,0x08,0x64,0x63}, ['-'-32]={0x08,0x08,0x08,0x08,0x08},
};static void set_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    uint8_t *byte = &framebuffer[(y / 8) * OLED_W + x];
    if (on) *byte |= BIT(y & 7);
    else *byte &= ~BIT(y & 7);
}
static void pixel(int x, int y) { set_pixel(x, y, true); }

static void hline(int x, int y, int width)
{
    for (int i = 0; i < width; i++) pixel(x + i, y);
}

static void vline(int x, int y, int height)
{
    for (int i = 0; i < height; i++) pixel(x, y + i);
}
static void fill_rect(int x, int y, int width, int height)
{
    for (int yy = y; yy < y + height; yy++)
        for (int xx = x; xx < x + width; xx++) pixel(xx, yy);
}
static void outline_rect(int x, int y, int width, int height)
{
    hline(x, y, width);
    hline(x, y + height - 1, width);
    vline(x, y, height);
    vline(x + width - 1, y, height);
}

static void draw_text_color(const char *s, int x, int y, bool foreground)
{
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        if (c == ' ') { x += 6; continue; }
        const uint8_t *g = NULL;
        if (c >= '0' && c <= '9') g = font[c - '0'];
        else if (c >= 32 && c < 128) g = font[c - 32];
        if (g != NULL) {
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 7; row++)
                    set_pixel(x + col, y + row, (g[col] & BIT(row)) ? foreground : !foreground);
        }
        x += 6;
    }
}
static void draw_text(const char *s, int x, int y)
{
    draw_text_color(s, x, y, true);
}

/* Double-sized 5x7 text uses 14 of the OLED's 32 vertical pixels. */
static void draw_large_text(const char *s, int x, int y)
{
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        if (c == ' ') { x += 12; continue; }
        const uint8_t *g = NULL;
        if (c >= '0' && c <= '9') g = font[c - '0'];
        else if (c >= 32 && c < 128) g = font[c - 32];
        if (g != NULL) {
            for (int col = 0; col < 5; col++) {
                for (int row = 0; row < 7; row++) {
                    bool on = (g[col] & BIT(row)) != 0;
                    set_pixel(x + col * 2, y + row * 2, on);
                    set_pixel(x + col * 2 + 1, y + row * 2, on);
                    set_pixel(x + col * 2, y + row * 2 + 1, on);
                    set_pixel(x + col * 2 + 1, y + row * 2 + 1, on);
                }
            }
        }
        x += 12;
    }
}

/* Exact 1-bit artwork copied from widgets/output_status_sym.c in the
 * established Hana 128x64 display. Rows are byte-aligned exactly as LVGL I1. */
static const uint8_t sym_usb_bits[28] = {
    0x7f,0x00, 0x41,0x00, 0x55,0x00, 0x41,0x00,
    0xff,0x80, 0x80,0x80, 0x80,0x80, 0x80,0x80,
    0x80,0x80, 0x80,0x80, 0x80,0x80, 0x80,0x80,
    0x80,0x80, 0xff,0x80
};
static const uint8_t sym_bt_bits[28] = {
    0x3e,0x00, 0x67,0x00, 0xe3,0x80, 0xe9,0x80,
    0x8c,0x80, 0xc9,0x80, 0xe3,0x80, 0xe3,0x80,
    0xc9,0x80, 0x8c,0x80, 0xe9,0x80, 0xe3,0x80,
    0x67,0x00, 0x3e,0x00
};
static const uint8_t sym_ok_bits[5]  = {0x08,0x18,0xb0,0xe0,0x40};
static const uint8_t sym_nok_bits[5] = {0x88,0xd8,0x70,0xd8,0x88};
static const uint8_t sym_open_bits[5]= {0x20,0x70,0xd8,0x70,0x20};
static const uint8_t sym_num_bits[5][6] = {
    {0x30,0x70,0x70,0x30,0x30,0x30},
    {0x70,0xd8,0x18,0x30,0x60,0xf8},
    {0x70,0x98,0x30,0x18,0xd8,0x70},
    {0x10,0x30,0x70,0xd0,0xf8,0x10},
    {0x78,0x40,0x70,0x18,0xd8,0x70}
};

/* LVGL I1 assets are MSB-first, with each row padded to a whole byte. */
static void draw_bitmap(int x, int y, int width, int height,
                        const uint8_t *bits)
{
    const int stride = (width + 7) / 8;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (bits[row * stride + col / 8] & (0x80u >> (col & 7))) {
                pixel(x + col, y + row);
            }
        }
    }
}

static void draw_usb_icon(int x, int y)
{
    draw_bitmap(x, y, 9, 14, sym_usb_bits);
}

static void draw_bluetooth_icon(int x, int y)
{
    draw_bitmap(x, y, 9, 14, sym_bt_bits);
}

/* The 64px selection underline becomes a two-pixel vertical marker
 * beside the active transport's icon in this stacked 32px layout. */
static void draw_active_transport_marker(int x, int y)
{
    vline(x, y, 10);
    vline(x + 1, y, 10);
}

static void draw_bt_state(int x, int y)
{
    const uint8_t *mark = !bt_bonded ? sym_open_bits :
                          (bt_connected ? sym_ok_bits : sym_nok_bits);
    draw_bitmap(x, y, 5, 5, mark);
}

static void draw_profile_number(int x, int y, uint8_t profile)
{
    if (profile < ARRAY_SIZE(sym_num_bits)) {
        draw_bitmap(x, y, 5, 6, sym_num_bits[profile]);
    } else {
        draw_bitmap(x, y, 5, 5, sym_open_bits);
    }
}

/* Compact 15x7 battery: sized for the dedicated 20px upper-right area. */
static void draw_battery_icon(int x, int y, uint8_t level)
{
    hline(x, y, 11);
    hline(x, y + 6, 11);
    vline(x, y, 7);
    vline(x + 10, y, 7);
    vline(x + 12, y + 2, 3);
    if (level != 0xff) {
        int fill = (MIN(level, 100) * 7 + 50) / 100;
        for (int xx = x + 2; xx < x + 2 + fill; xx++)
            for (int yy = y + 2; yy < y + 5; yy++) pixel(xx, yy);
    }
}

static void draw_battery_row(int y, char side, uint8_t level)
{
    /* Four glyphs occupy 23px; the compact battery sits at the far right. */
    char label[5] = { side, ' ', '-', '-', 0 };

    if (level != 0xff) {
        const unsigned int value = MIN(level, 100);
        if (value >= 100) {
            label[1] = '1';
            label[2] = '0';
            label[3] = '0';
        } else {
            label[2] = '0' + (value / 10);
            label[3] = '0' + (value % 10);
        }
    }

    draw_text(label, 79, y);
    draw_battery_icon(109, y, level);
}

/* Exact 14x14 modifier glyphs from the established Hana 128x64
 * modifiers_sym.c assets. Each row is two bytes, MSB first. */
static const uint8_t modifier_control_bits[28] = {
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x80, 0x0c, 0xc0, 0x18, 0x60, 0x30, 0x30,
    0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t modifier_shift_bits[28] = {
    0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x80, 0x0c, 0xc0, 0x18, 0x60, 0x30, 0x30,
    0x78, 0x78, 0x08, 0x40, 0x08, 0x40, 0x08, 0x40, 0x0f, 0xc0, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t modifier_opt_bits[28] = {
    0x00, 0x00, 0x00, 0x00, 0x3c, 0xe0, 0x3c, 0xe0, 0x06, 0x00, 0x06, 0x00, 0x06, 0x00,
    0x03, 0x00, 0x03, 0x00, 0x03, 0x00, 0x01, 0xe0, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t modifier_cmd_bits[28] = {
    0x00, 0x00, 0x00, 0x00, 0x18, 0x60, 0x24, 0x90, 0x24, 0x90, 0x1f, 0xe0, 0x04, 0x80,
    0x04, 0x80, 0x1f, 0xe0, 0x24, 0x90, 0x24, 0x90, 0x18, 0x60, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t modifier_alt_bits[28] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0xf8, 0x4a, 0x20, 0x4a, 0x20,
    0x7a, 0x20, 0x4a, 0x20, 0x4b, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t modifier_win_bits[28] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0b, 0xf0, 0x3b, 0xf0, 0x3b, 0xf0, 0x3b, 0xf0,
    0x00, 0x00, 0x3b, 0xf0, 0x3b, 0xf0, 0x3b, 0xf0, 0x03, 0xf0, 0x00, 0x30, 0x00, 0x00
};

static void draw_modifier_bitmap(int x, int y, const uint8_t *bits, bool inverted)
{
    for (int row = 0; row < 14; row++) {
        for (int col = 0; col < 14; col++) {
            const bool set = (bits[row * 2 + col / 8] & (0x80u >> (col & 7))) != 0;
            set_pixel(x + col, y + row, inverted ? !set : set);
        }
    }
}

/* Show only active host locks in the lower-right strip.
 * When no locks are active, this region remains completely clear. */
static void draw_lock_indicators(void)
{
    static const struct {
        uint8_t mask;
        char letter;
    } locks[] = {
        { HID_INDICATOR_CAPS_LOCK, 'C' },
        { HID_INDICATOR_NUM_LOCK, 'N' },
        { HID_INDICATOR_SCROLL_LOCK, 'S' },
    };

    const int box_size = 12;
    const int gap = 2;
    const int y = OLED_H - box_size;
    int active_count = 0;

    for (size_t i = 0; i < ARRAY_SIZE(locks); i++) {
        if (lock_indicators & locks[i].mask) active_count++;
    }
    if (active_count == 0) return;

    const int total_width = active_count * box_size +
                            (active_count - 1) * gap;
    int x = OLED_W - total_width - 1;

    for (size_t i = 0; i < ARRAY_SIZE(locks); i++) {
        if (!(lock_indicators & locks[i].mask)) continue;

        outline_rect(x, y, box_size, box_size);
        char label[] = { locks[i].letter, '\0' };
        draw_text(label, x + 3, y + 2);
        x += box_size + gap;
    }
}

static int dashboard_flush(void)
{
    const struct display_buffer_descriptor desc = {
        .buf_size = sizeof(framebuffer), .width = OLED_W,
        .height = OLED_H, .pitch = OLED_W,
    };
    return display_write(oled, 0, 0, &desc, framebuffer);
}

static void draw_layer_screen(void)
{
    const int y = (OLED_H - 14) / 2;
    const int width = (int)strlen(active_layer_name) * 12;
    if (width <= OLED_W) {
        draw_large_text(active_layer_name, (OLED_W - width) / 2, y);
    } else {
        const int cycle = width + 24;
        const int offset = layer_scroll_px % cycle;
        draw_large_text(active_layer_name, -offset, y);
        draw_large_text(active_layer_name, width + 24 - offset, y);
    }
}

static void draw_timeout_feedback(void)
{
    const char *title = timeout_feedback_kind == ZMK_RUNTIME_TIMEOUT_DISPLAY
                            ? "DISPLAY" : "HALF SLEEP";
    char value[16];
    if (timeout_feedback_ms == 0) {
        snprintk(value, sizeof(value), "NEVER");
    } else if (timeout_feedback_kind == ZMK_RUNTIME_TIMEOUT_DISPLAY) {
        snprintk(value, sizeof(value), "%us", timeout_feedback_ms / 1000);
    } else {
        snprintk(value, sizeof(value), "%um", timeout_feedback_ms / 60000);
    }

    const int title_width = (int)strlen(title) * 12;
    const int value_width = (int)strlen(value) * 12;
    draw_large_text(title, (OLED_W - title_width) / 2, 1);
    draw_large_text(value, (OLED_W - value_width) / 2, 17);
}

static void draw_modifier_family(void)
{
    const uint8_t masks[4] = {
        MOD_LCTL | MOD_RCTL,
        layout_windows_mode ? (MOD_LGUI | MOD_RGUI) : (MOD_LALT | MOD_RALT),
        layout_windows_mode ? (MOD_LALT | MOD_RALT) : (MOD_LGUI | MOD_RGUI),
        MOD_LSFT | MOD_RSFT,
    };
    const uint8_t *const mac_icons[4] = {
        modifier_control_bits, modifier_opt_bits, modifier_cmd_bits, modifier_shift_bits,
    };
    const uint8_t *const win_icons[4] = {
        modifier_control_bits, modifier_win_bits, modifier_alt_bits, modifier_shift_bits,
    };
    const uint8_t *const *icons = layout_windows_mode ? win_icons : mac_icons;
    const uint8_t active = zmk_hid_get_explicit_mods();

    /* Four 32px slots, each holding the original 14x14 Hana glyph. */
    for (int i = 0; i < 4; i++) {
        const int x = i * 32 + 9;
        const bool pressed = (active & masks[i]) != 0;
        draw_modifier_bitmap(x, 8, icons[i], pressed);
        if (pressed) {
            /* A short underline reinforces the inverted icon state. */
            hline(x, 25, 14);
            hline(x, 26, 14);
        }
    }
}

static int dashboard_render(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));

    /* Timeout feedback takes priority over layers and dashboard views. */
    if (timeout_feedback_active) {
        draw_timeout_feedback();
        return dashboard_flush();
    }

    /* Any active non-base layer owns the screen until it is released. */
    if (active_layer != 0) {
        draw_layer_screen();
        return dashboard_flush();
    }

    /* Brief full-screen OS confirmation, matching the bordered layer badge
     * treatment used by the established Hana 128x64 status UI. */
    if (layout_flash_active) {
        const char *label = layout_windows_mode ? "WIN" : "MAC";
        const int width = (int)strlen(label) * 12;
        outline_rect(1, 1, OLED_W - 2, OLED_H - 2);
        draw_large_text(label, (OLED_W - width) / 2, (OLED_H - 14) / 2);
        return dashboard_flush();
    }

    if (base_view_mode == 1) {
        draw_modifier_family();
        return dashboard_flush();
    }

    /* Original, tested connection and battery dashboard. */
    draw_usb_icon(7, 2);
    draw_bluetooth_icon(7, 17);
    draw_bt_state(17, 17);
    draw_profile_number(17, 25, bt_profile);

    if (active_transport == ZMK_TRANSPORT_USB) {
        draw_active_transport_marker(1, 4);
    } else if (active_transport == ZMK_TRANSPORT_BLE) {
        draw_active_transport_marker(1, 19);
    }

    /* Two compact battery rows occupy the upper 20px of the right panel. */
    draw_battery_row(1, 'L', half_level[0]);
    draw_battery_row(10, 'R', half_level[1]);
    draw_lock_indicators();
    return dashboard_flush();
}

static void layer_scroll_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    if (active_layer == 0) return;

    const int width = (int)strlen(active_layer_name) * 12;
    if (width <= OLED_W) return;

    const int cycle = width + 24;
    layer_scroll_px++;
    if (layer_scroll_px >= cycle) {
        layer_scroll_px = 0;
        (void)dashboard_render();
        (void)k_work_reschedule(&layer_scroll_work, K_MSEC(650));
        return;
    }

    (void)dashboard_render();
    (void)k_work_reschedule(&layer_scroll_work, K_MSEC(35));
}

static void timeout_feedback_timeout_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    timeout_feedback_active = false;
    (void)dashboard_render();
}

static void layout_flash_timeout_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    layout_flash_active = false;
    (void)dashboard_render();
}

static void layout_probe_handler(struct k_work *work)
{
    ARG_UNUSED(work);
    const bool next_mode = read_layout_windows_mode();
    if (next_mode != layout_windows_mode) {
        layout_windows_mode = next_mode;
        layout_flash_active = true;
        (void)k_work_reschedule(&layout_flash_work, K_MSEC(700));
    }
    (void)dashboard_render();
}

static void refresh_layer_state(void)
{
    const uint8_t next_layer = zmk_keymap_highest_layer_active();
    if (next_layer == active_layer) return;

    active_layer = next_layer;
    layer_scroll_px = 0;
    /* active_layer is an ordering index; layer_name expects the stable ID. */
    const zmk_keymap_layer_id_t layer_id = zmk_keymap_layer_index_to_id(active_layer);
    const char *name = zmk_keymap_layer_name(layer_id);
    if (name == NULL || name[0] == '\0') {
        snprintk(active_layer_name, sizeof(active_layer_name), "LAYER %u", active_layer);
    } else {
        snprintk(active_layer_name, sizeof(active_layer_name), "%s", name);
    }

    (void)k_work_cancel_delayable(&layer_scroll_work);
    if (active_layer != 0 && strlen(active_layer_name) * 12 > OLED_W) {
        (void)k_work_reschedule(&layer_scroll_work, K_MSEC(650));
    }
    (void)dashboard_render();
}

static void refresh_output_state(void)
{
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();
    active_transport = endpoint.transport;
    if (active_transport == ZMK_TRANSPORT_NONE) {
        active_transport = zmk_endpoint_get_preferred_transport();
    }
    bt_connected = zmk_ble_active_profile_is_connected();
    bt_bonded = !zmk_ble_active_profile_is_open();
    bt_profile = zmk_ble_active_profile_index();
    (void)dashboard_render();
}

static int dashboard_listener(const zmk_event_t *eh)
{
    const struct zmk_activity_state_changed *activity =
        as_zmk_activity_state_changed(eh);
    if (activity != NULL) {
        if (activity->state == ZMK_ACTIVITY_ACTIVE) {
            raw_manual_display_off = false;
        }
        raw_apply_display_power();
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_runtime_timeout_changed *timeout =
        as_zmk_runtime_timeout_changed(eh);
    if (timeout != NULL) {
        if (timeout->feedback == ZMK_RUNTIME_TIMEOUT_FEEDBACK_HIDE) {
            timeout_feedback_active = false;
            (void)k_work_cancel_delayable(&timeout_feedback_work);
        } else {
            timeout_feedback_active = true;
            timeout_feedback_kind = timeout->kind;
            timeout_feedback_ms = timeout->timeout_ms;
            if (timeout->feedback == ZMK_RUNTIME_TIMEOUT_FEEDBACK_TIMED) {
                (void)k_work_reschedule(&timeout_feedback_work, K_MSEC(3000));
            } else {
                (void)k_work_cancel_delayable(&timeout_feedback_work);
            }
        }
        (void)dashboard_render();
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_layer_state_changed(eh) != NULL) {
        refresh_layer_state();
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_hid_indicators_changed *locks =
        as_zmk_hid_indicators_changed(eh);
    if (locks != NULL) {
        lock_indicators = locks->indicators;
        (void)dashboard_render();
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_keycode_state_changed(eh) != NULL ||
        as_zmk_position_state_changed(eh) != NULL) {
        /* Read the map after the layout-shift behavior has updated its state. */
        (void)k_work_reschedule(&layout_probe_work, K_MSEC(20));
        (void)dashboard_render();
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_peripheral_battery_state_changed *battery =
        as_zmk_peripheral_battery_state_changed(eh);
    if (battery != NULL) {
        if (battery->source < ARRAY_SIZE(half_level)) {
            half_level[battery->source] = battery->state_of_charge;
        }
        (void)dashboard_render();
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_endpoint_changed(eh) != NULL ||
        as_zmk_usb_conn_state_changed(eh) != NULL ||
        as_zmk_ble_active_profile_changed(eh) != NULL) {
        refresh_output_state();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(hana_128x32_dashboard, dashboard_listener);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_endpoint_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_usb_conn_state_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_position_state_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_hid_indicators_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_runtime_timeout_changed);
ZMK_SUBSCRIPTION(hana_128x32_dashboard, zmk_activity_state_changed);

static int hana_128x32_dashboard_init(void)
{
    if (!device_is_ready(oled)) {
        LOG_ERR("128x32 OLED device is not ready");
        return -ENODEV;
    }
    struct display_capabilities caps;
    display_get_capabilities(oled, &caps);
    if (caps.x_resolution != OLED_W || caps.y_resolution != OLED_H ||
        !(caps.screen_info & SCREEN_INFO_MONO_VTILED) ||
        !(caps.supported_pixel_formats & PIXEL_FORMAT_MONO01)) {
        LOG_ERR("Unexpected OLED geometry/format: %ux%u info=%x formats=%x",
                caps.x_resolution, caps.y_resolution,
                caps.screen_info, caps.supported_pixel_formats);
        return -ENOTSUP;
    }
    int err = display_set_pixel_format(oled, PIXEL_FORMAT_MONO01);
    if (err) return err;
    raw_apply_display_power();

    k_work_init_delayable(&layer_scroll_work, layer_scroll_handler);
    k_work_init_delayable(&layout_probe_work, layout_probe_handler);
    k_work_init_delayable(&layout_flash_work, layout_flash_timeout_handler);
    k_work_init_delayable(&timeout_feedback_work, timeout_feedback_timeout_handler);
    layout_windows_mode = read_layout_windows_mode();
    lock_indicators = zmk_hid_indicators_get_current_profile();
    refresh_output_state();
    refresh_layer_state();
    LOG_INF("128x32 base status renderer initialized");
    return 0;
}
SYS_INIT(hana_128x32_dashboard_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
