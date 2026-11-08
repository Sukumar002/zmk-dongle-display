/*
 * SPDX-License-Identifier: MIT
 *
 * Hana 128x32 base status screen.
 * This renderer is the sole OLED writer in the raw-dashboard build:
 * no LVGL screen, canvas, or ZMK display widget is initialized.
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
#define HALF_COUNT CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS

static const struct device *const oled = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static uint8_t framebuffer[FB_SIZE];
static uint8_t half_level[2] = { 0xff, 0xff };
static bool usb_ready;
static bool bt_connected;
static uint8_t bt_profile;

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

/* Compact 3x5 alphanumeric font: digits, L/R/U/S/B and dash/percent. */
static const uint8_t glyphs[][3] = {
    {0x1f,0x11,0x1f}, {0x00,0x1f,0x00}, {0x1d,0x15,0x17},
    {0x15,0x15,0x1f}, {0x07,0x04,0x1f}, {0x17,0x15,0x1d},
    {0x1f,0x15,0x1d}, {0x01,0x01,0x1f}, {0x1f,0x15,0x1f},
    {0x17,0x15,0x1f}, /* 0..9 */
    {0x1f,0x10,0x10}, /* L */
    {0x1f,0x05,0x1a}, /* R */
    {0x1f,0x05,0x1f}, /* U */
    {0x1f,0x05,0x1f}, /* S */
    {0x1f,0x05,0x02}, /* B */
    {0x04,0x04,0x04}, /* - */
    {0x19,0x04,0x13}, /* % */
};
static int glyph_index(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    switch (c) {
    case 'L': return 10; case 'R': return 11; case 'U': return 12;
    case 'S': return 13; case 'B': return 14; case '-': return 15;
    case '%': return 16; default: return -1;
    }
}
static void draw_text(const char *s, int x, int y)
{
    while (*s) {
        int g = glyph_index(*s++);
        if (g >= 0) {
            for (int col = 0; col < 3; col++)
                for (int row = 0; row < 5; row++)
                    if (glyphs[g][col] & BIT(row)) pixel(x + col, y + row);
        }
        x += 4;
    }
}

static void draw_usb_icon(void)
{
    /* USB trident, deliberately simple and legible at 8x10. */
    vline(5, 1, 9);
    hline(2, 2, 7);
    pixel(5, 0); pixel(4, 1); pixel(6, 1);
    pixel(2, 2); pixel(1, 3); pixel(3, 3);
    pixel(8, 2); pixel(7, 3); pixel(9, 3);
    pixel(5, 9); pixel(4, 8); pixel(6, 8);
}

static void draw_bluetooth_icon(void)
{
    /* Standard Bluetooth rune, 8x12. */
    vline(5, 19, 12);
    pixel(5,19); pixel(6,20); pixel(7,21); pixel(6,22);
    pixel(5,23); pixel(6,24); pixel(7,25); pixel(6,26);
    pixel(5,30);
    pixel(2,21); pixel(3,22); pixel(4,23);
    pixel(2,28); pixel(3,27); pixel(4,26);
}

static void draw_battery_row(int y, char side, uint8_t level)
{
    char label[5];
    if (level == 0xff) {
        label[0] = side; label[1] = '-'; label[2] = '-'; label[3] = 0;
    } else {
        unsigned int value = MIN(level, 100);
        if (value == 100) {
            label[0] = side; label[1] = '1'; label[2] = '0'; label[3] = '0';
            label[4] = 0;
        } else {
            label[0] = side; label[1] = '0' + (value / 10);
            label[2] = '0' + (value % 10); label[3] = '%'; label[4] = 0;
        }
    }
    draw_text(label, 92, y + 2);

    /* Battery outline and terminal; fill is proportional to charge. */
    hline(116, y, 9); hline(116, y + 8, 9);
    vline(116, y, 9); vline(124, y, 9);
    vline(125, y + 2, 5);
    if (level != 0xff) {
        int fill = (MIN(level, 100) * 6) / 100;
        for (int x = 118; x < 118 + fill; x++)
            for (int yy = y + 2; yy < y + 7; yy++) pixel(x, yy);
    }
}

static int dashboard_render(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));

    draw_usb_icon();
    if (usb_ready) pixel(10, 1);
    draw_text("USB", 14, 3);

    draw_bluetooth_icon();
    if (bt_connected) pixel(10, 20);
    draw_text("B", 14, 22);
    draw_text(bt_profile < 9 ? (bt_profile == 0 ? "1" :
              bt_profile == 1 ? "2" : bt_profile == 2 ? "3" :
              bt_profile == 3 ? "4" : "5") : "1", 19, 22);

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
    usb_ready = zmk_usb_is_hid_ready() &&
                endpoint.transport == ZMK_TRANSPORT_USB;
    bt_connected = zmk_ble_active_profile_is_connected();
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
