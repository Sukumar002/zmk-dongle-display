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
#include <zephyr/logging/log.h>
#include <string.h>

#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/usb.h>
#include <zmk/ble.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/battery_state_changed.h>

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

/* 5x7 column font. Bit zero is the top row. */
static const uint8_t font[][5] = {
    [0]  = {0x3e,0x51,0x49,0x45,0x3e}, /* 0 */
    [1]  = {0x00,0x42,0x7f,0x40,0x00},
    [2]  = {0x42,0x61,0x51,0x49,0x46},
    [3]  = {0x21,0x41,0x45,0x4b,0x31},
    [4]  = {0x18,0x14,0x12,0x7f,0x10},
    [5]  = {0x27,0x45,0x45,0x45,0x39},
    [6]  = {0x3c,0x4a,0x49,0x49,0x30},
    [7]  = {0x01,0x71,0x09,0x05,0x03},
    [8]  = {0x36,0x49,0x49,0x49,0x36},
    [9]  = {0x06,0x49,0x49,0x29,0x1e},
    ['L' - 32] = {0x7f,0x40,0x40,0x40,0x40},
    ['R' - 32] = {0x7f,0x09,0x19,0x29,0x46},
    ['U' - 32] = {0x3f,0x40,0x40,0x40,0x3f},
    ['S' - 32] = {0x26,0x49,0x49,0x49,0x32},
    ['B' - 32] = {0x7f,0x49,0x49,0x49,0x36},
    ['%' - 32] = {0x63,0x13,0x08,0x64,0x63},
    ['-' - 32] = {0x08,0x08,0x08,0x08,0x08},
};

static void pixel(int x, int y)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    framebuffer[(y / 8) * OLED_W + x] |= BIT(y & 7);
}

static void hline(int x, int y, int width)
{
    for (int i = 0; i < width; i++) pixel(x + i, y);
}

static void vline(int x, int y, int height)
{
    for (int i = 0; i < height; i++) pixel(x, y + i);
}

static void draw_text(const char *s, int x, int y)
{
    while (*s) {
        unsigned char c = (unsigned char)*s++;
        if (c >= '0' && c <= '9') {
            const uint8_t *g = font[c - '0'];
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 7; row++)
                    if (g[col] & BIT(row)) pixel(x + col, y + row);
        } else if (c >= 32 && c < 128) {
            const uint8_t *g = font[c - 32];
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 7; row++)
                    if (g[col] & BIT(row)) pixel(x + col, y + row);
        }
        x += 6;
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

/* Each battery is 17x10, with a substantial six-pixel charge window. */
static void draw_battery_icon(int x, int y, uint8_t level)
{
    hline(x, y, 14);
    hline(x, y + 9, 14);
    vline(x, y, 10);
    vline(x + 13, y, 10);
    vline(x + 15, y + 3, 4);
    if (level != 0xff) {
        int fill = (MIN(level, 100) * 10 + 50) / 100;
        for (int xx = x + 2; xx < x + 2 + fill; xx++)
            for (int yy = y + 2; yy < y + 8; yy++) pixel(xx, yy);
    }
}

static void draw_battery_row(int y, char side, uint8_t level)
{
    char label[6];
    if (level == 0xff) {
        label[0] = side; label[1] = '-'; label[2] = '-'; label[3] = 0;
    } else {
        unsigned int value = MIN(level, 100);
        if (value == 100) {
            label[0] = side; label[1] = '1'; label[2] = '0';
            label[3] = '0'; label[4] = '%'; label[5] = 0;
        } else {
            label[0] = side; label[1] = '0' + (value / 10);
            label[2] = '0' + (value % 10); label[3] = '%'; label[4] = 0;
        }
    }
    draw_text(label, 70, y + 1);
    draw_battery_icon(108, y, level);
}

static int dashboard_render(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));

    /* USB and Bluetooth retain the 64px artwork and status semantics.
     * The selected-output underline is transposed into a vertical marker. */
    draw_usb_icon(7, 2);
    draw_bluetooth_icon(7, 17);
    draw_profile_number(17, 25, bt_profile);
    draw_bt_state(25, 21);

    if (active_transport == ZMK_TRANSPORT_USB) {
        draw_active_transport_marker(1, 4);
    } else if (active_transport == ZMK_TRANSPORT_BLE) {
        draw_active_transport_marker(1, 19);
    }

    draw_battery_row(2, 'L', half_level[0]);
    draw_battery_row(18, 'R', half_level[1]);

    const struct display_buffer_descriptor desc = {
        .buf_size = sizeof(framebuffer), .width = OLED_W,
        .height = OLED_H, .pitch = OLED_W,
    };
    return display_write(oled, 0, 0, &desc, framebuffer);
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
    err = display_blanking_off(oled);
    if (err) LOG_WRN("display_blanking_off returned %d", err);

    refresh_output_state();
    LOG_INF("128x32 base status renderer initialized");
    return 0;
}
SYS_INIT(hana_128x32_dashboard_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
