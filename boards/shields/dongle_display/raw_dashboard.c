/*
 * SPDX-License-Identifier: MIT
 *
 * Full-panel diagnostic for the Hana 128x32 SSD1306.
 * Uses Zephyr's native MONO01, vertically tiled display buffer format.
 * No LVGL and no ZMK status widgets are involved.
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(hana_oled_fullscreen_test, LOG_LEVEL_INF);

#define OLED_W 128
#define OLED_H 32
#define FB_SIZE (OLED_W * OLED_H / 8)

static const struct device *const display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static uint8_t framebuffer[FB_SIZE];

static void pixel(int x, int y)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    /* SSD1306 Zephyr driver advertises SCREEN_INFO_MONO_VTILED:
     * each byte contains eight vertical pixels, least-significant first.
     */
    framebuffer[(y / 8) * OLED_W + x] |= BIT(y & 7);
}

static void line(int x0, int y0, int x1, int y1)
{
    int dx = ABS(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -ABS(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Five-column, seven-row glyphs for the diagnostic label. */
static const uint8_t font[][5] = {
    [0] = {0x3e,0x51,0x49,0x45,0x3e}, /* 0 */
    [1] = {0x00,0x42,0x7f,0x40,0x00}, /* 1 */
    [2] = {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    [3] = {0x21,0x41,0x45,0x4b,0x31}, /* 3 */
    [10] = {0x7e,0x11,0x11,0x11,0x7e}, /* A */
    [11] = {0x7f,0x49,0x49,0x49,0x36}, /* B */
    [12] = {0x3e,0x41,0x41,0x41,0x22}, /* C */
    [13] = {0x7f,0x41,0x41,0x22,0x1c}, /* D */
    [14] = {0x7f,0x49,0x49,0x49,0x41}, /* E */
    [15] = {0x7f,0x09,0x09,0x09,0x01}, /* F */
    [16] = {0x3e,0x41,0x49,0x49,0x7a}, /* G */
    [17] = {0x7f,0x08,0x08,0x08,0x7f}, /* H */
    [18] = {0x00,0x41,0x7f,0x41,0x00}, /* I */
    [19] = {0x20,0x40,0x41,0x3f,0x01}, /* J */
    [20] = {0x7f,0x08,0x14,0x22,0x41}, /* K */
    [21] = {0x7f,0x40,0x40,0x40,0x40}, /* L */
    [22] = {0x7f,0x02,0x0c,0x02,0x7f}, /* M */
    [23] = {0x7f,0x04,0x08,0x10,0x7f}, /* N */
    [24] = {0x3e,0x41,0x41,0x41,0x3e}, /* O */
    [25] = {0x7f,0x09,0x09,0x09,0x06}, /* P */
    [26] = {0x7f,0x09,0x19,0x29,0x46}, /* R */
    [27] = {0x46,0x49,0x49,0x49,0x31}, /* S */
    [28] = {0x01,0x01,0x7f,0x01,0x01}, /* T */
    [29] = {0x3f,0x40,0x40,0x40,0x3f}, /* U */
    [30] = {0x1f,0x20,0x40,0x20,0x1f}, /* V */
    [31] = {0x63,0x14,0x08,0x14,0x63}, /* X */
};

static int glyph_index(char c)
{
    if (c >= '0' && c <= '3') return c - '0';
    switch (c) {
    case 'A': return 10; case 'B': return 11; case 'C': return 12;
    case 'D': return 13; case 'E': return 14; case 'F': return 15;
    case 'G': return 16; case 'H': return 17; case 'I': return 18;
    case 'J': return 19; case 'K': return 20; case 'L': return 21;
    case 'M': return 22; case 'N': return 23; case 'O': return 24;
    case 'P': return 25; case 'R': return 26; case 'S': return 27;
    case 'T': return 28; case 'U': return 29; case 'V': return 30;
    case 'X': return 31;
    default: return -1;
    }
}

static void text(const char *s, int x, int y)
{
    while (*s) {
        int g = glyph_index(*s++);
        if (g >= 0) {
            for (int col = 0; col < 5; col++)
                for (int row = 0; row < 7; row++)
                    if (font[g][col] & BIT(row)) pixel(x + col, y + row);
        }
        x += 6;
    }
}

static int draw_fullscreen_test(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));

    /* Full perimeter, corner blocks, centre axes and diagonals. */
    line(0, 0, 127, 0);
    line(0, 31, 127, 31);
    line(0, 0, 0, 31);
    line(127, 0, 127, 31);

    for (int y = 2; y < 7; y++)
        for (int x = 2; x < 7; x++) pixel(x, y);
    for (int y = 2; y < 7; y++)
        for (int x = 121; x < 126; x++) pixel(x, y);
    for (int y = 25; y < 30; y++)
        for (int x = 2; x < 7; x++) pixel(x, y);
    for (int y = 25; y < 30; y++)
        for (int x = 121; x < 126; x++) pixel(x, y);

    line(8, 8, 119, 23);
    line(8, 23, 119, 8);
    line(64, 1, 64, 30);
    line(1, 15, 126, 15);

    /* Clear a central label window, then draw legible 5x7 text. */
    for (int y = 10; y <= 21; y++)
        for (int x = 31; x <= 96; x++)
            framebuffer[(y / 8) * OLED_W + x] &= (uint8_t)~BIT(y & 7);
    text("128X32", 46, 12);

    const struct display_buffer_descriptor desc = {
        .buf_size = sizeof(framebuffer),
        .width = OLED_W,
        .height = OLED_H,
        .pitch = OLED_W,
    };
    return display_write(display, 0, 0, &desc, framebuffer);
}

static int hana_oled_fullscreen_test_init(void)
{
    if (!device_is_ready(display)) {
        LOG_ERR("Chosen OLED display device is not ready");
        return -ENODEV;
    }

    struct display_capabilities caps;
    display_get_capabilities(display, &caps);
    LOG_INF("OLED diagnostic: %ux%u formats=0x%x screen_info=0x%x",
            caps.x_resolution, caps.y_resolution,
            caps.supported_pixel_formats, caps.screen_info);

    if (caps.x_resolution != OLED_W || caps.y_resolution != OLED_H) {
        LOG_ERR("Expected 128x32 panel; refusing to draw");
        return -EINVAL;
    }
    if (!(caps.screen_info & SCREEN_INFO_MONO_VTILED) ||
        !(caps.supported_pixel_formats & PIXEL_FORMAT_MONO01)) {
        LOG_ERR("Unexpected SSD1306 buffer format");
        return -ENOTSUP;
    }

    int err = display_set_pixel_format(display, PIXEL_FORMAT_MONO01);
    if (err) {
        LOG_ERR("Could not select MONO01: %d", err);
        return err;
    }
    err = display_blanking_off(display);
    if (err) LOG_WRN("display_blanking_off: %d", err);

    err = draw_fullscreen_test();
    if (err) LOG_ERR("Full-screen display_write failed: %d", err);
    else LOG_INF("Full-screen 128x32 diagnostic frame written");
    return err;
}

SYS_INIT(hana_oled_fullscreen_test_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
