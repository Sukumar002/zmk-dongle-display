/*
 * SPDX-License-Identifier: MIT
 *
 * Minimal 128x32 SSD1306 baseline. This deliberately bypasses LVGL so the
 * Zephyr display driver's native monochrome framebuffer path can be verified.
 */
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(hana_raw_dashboard, LOG_LEVEL_INF);

#define OLED_W 128
#define OLED_H 32
#define FB_SIZE (OLED_W * OLED_H / 8)

static const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
static uint8_t framebuffer[FB_SIZE];

/* 5x7 row-major glyphs. Bit 0 is the left-most pixel. */
static const uint8_t glyphs[][7] = {
    [0] = {0x1f,0x11,0x11,0x11,0x11,0x11,0x1f}, /* 0 */
    [1] = {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
    [2] = {0x1e,0x01,0x01,0x1e,0x10,0x10,0x1f},
    [3] = {0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e},
    [4] = {0x12,0x12,0x12,0x1f,0x02,0x02,0x02},
    [5] = {0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e},
    [6] = {0x0f,0x10,0x10,0x1e,0x11,0x11,0x0e},
    [7] = {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
    [8] = {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},
    [9] = {0x0e,0x11,0x11,0x0f,0x01,0x01,0x1e},
    [10] = {0x0e,0x11,0x11,0x1f,0x11,0x11,0x11}, /* A */
    [11] = {0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e}, /* B */
    [12] = {0x0f,0x10,0x10,0x10,0x10,0x10,0x0f}, /* C */
    [13] = {0x1e,0x11,0x11,0x11,0x11,0x11,0x1e}, /* D */
    [14] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f}, /* E */
    [15] = {0x1f,0x10,0x10,0x1e,0x10,0x10,0x10}, /* F */
    [16] = {0x0f,0x10,0x10,0x13,0x11,0x11,0x0f}, /* G */
    [17] = {0x11,0x11,0x11,0x1f,0x11,0x11,0x11}, /* H */
    [18] = {0x0e,0x04,0x04,0x04,0x04,0x04,0x0e}, /* I */
    [19] = {0x07,0x02,0x02,0x02,0x12,0x12,0x0c}, /* J */
    [20] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    [21] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1f}, /* L */
    [22] = {0x11,0x1b,0x15,0x15,0x11,0x11,0x11}, /* M */
    [23] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    [24] = {0x0e,0x11,0x11,0x11,0x11,0x11,0x0e}, /* O */
    [25] = {0x1e,0x11,0x11,0x1e,0x10,0x10,0x10}, /* P */
    [26] = {0x1e,0x11,0x11,0x1e,0x14,0x12,0x11}, /* R */
    [27] = {0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e}, /* S */
    [28] = {0x1f,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    [29] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0e}, /* U */
};

static void put_pixel(int x, int y, bool on)
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    const size_t index = (size_t)(y / 8) * OLED_W + x;
    const uint8_t mask = BIT(y & 7);
    if (on) framebuffer[index] |= mask;
    else framebuffer[index] &= (uint8_t)~mask;
}

static void draw_char(char c, int x, int y)
{
    int glyph = -1;
    if (c >= '0' && c <= '9') glyph = c - '0';
    else {
        switch (c) {
        case 'A': glyph=10; break; case 'B': glyph=11; break;
        case 'C': glyph=12; break; case 'D': glyph=13; break;
        case 'E': glyph=14; break; case 'F': glyph=15; break;
        case 'G': glyph=16; break; case 'H': glyph=17; break;
        case 'I': glyph=18; break; case 'J': glyph=19; break;
        case 'K': glyph=20; break; case 'L': glyph=21; break;
        case 'M': glyph=22; break; case 'N': glyph=23; break;
        case 'O': glyph=24; break; case 'P': glyph=25; break;
        case 'R': glyph=26; break; case 'S': glyph=27; break;
        case 'T': glyph=28; break; case 'U': glyph=29; break;
        case '-': for (int i=0;i<5;i++) put_pixel(x+i,y+3,true); return;
        case '+': for (int i=0;i<5;i++) { put_pixel(x+i,y+3,true); put_pixel(x+2,y+1,true); put_pixel(x+2,y+5,true); } return;
        case '%':
            put_pixel(x+1,y+1,true); put_pixel(x+2,y+1,true);
            put_pixel(x+1,y+2,true); put_pixel(x+2,y+2,true);
            put_pixel(x+3,y+4,true); put_pixel(x+3,y+5,true);
            put_pixel(x+4,y+4,true); put_pixel(x+4,y+5,true);
            put_pixel(x+1,y+6,true); put_pixel(x+4,y,true); return;
        default: return;
        }
    }
    for (int row=0; row<7; row++)
        for (int col=0; col<5; col++)
            if (glyphs[glyph][row] & BIT(col)) put_pixel(x+col,y+row,true);
}

static void draw_text(const char *s, int x, int y)
{
    while (*s) {
        if (*s != ' ') draw_char(*s, x, y);
        x += 6;
        s++;
    }
}

static void draw_battery(int x, int y, uint8_t percent)
{
    /* 7x10 outline with a one-pixel terminal. */
    for (int xx=0; xx<6; xx++) {
        put_pixel(x+xx,y,true);
        put_pixel(x+xx,y+9,true);
    }
    for (int yy=0; yy<10; yy++) {
        put_pixel(x,y+yy,true);
        put_pixel(x+5,y+yy,true);
    }
    put_pixel(x+2,y-1,true); put_pixel(x+3,y-1,true);
    int fill = (percent > 100 ? 100 : percent) * 6 / 100;
    for (int yy=0; yy<fill; yy++)
        for (int xx=0; xx<3; xx++)
            put_pixel(x+2+xx,y+8-yy,true);
}

static int flush_frame(void)
{
    struct display_buffer_descriptor desc = {
        .buf_size = sizeof(framebuffer),
        .width = OLED_W,
        .height = OLED_H,
        .pitch = OLED_W,
    };
    return display_write(display, 0, 0, &desc, framebuffer);
}

static void render_baseline(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));

    /* Connection indicators: top-left USB, bottom-left Bluetooth. */
    draw_text("USB", 1, 2);
    draw_text("+", 22, 2);
    draw_text("BT 1", 1, 22);
    draw_text("-", 31, 22);

    /* Two split-half battery rows, stacked at the far right. */
    draw_text("L --%", 76, 1);
    draw_battery(119, 4, 0);
    draw_text("R --%", 76, 17);
    draw_battery(119, 20, 0);

    int err = flush_frame();
    if (err) LOG_ERR("SSD1306 frame write failed: %d", err);
}

static int hana_raw_dashboard_init(void)
{
    if (!device_is_ready(display)) {
        LOG_ERR("Chosen SSD1306 display device is not ready");
        return -ENODEV;
    }

    struct display_capabilities caps;
    display_get_capabilities(display, &caps);
    LOG_INF("Raw dashboard display: %ux%u formats=0x%x screen_info=0x%x",
            caps.x_resolution, caps.y_resolution,
            caps.supported_pixel_formats, caps.screen_info);

    if (caps.x_resolution != OLED_W || caps.y_resolution != OLED_H) {
        LOG_ERR("Raw dashboard requires a 128x32 display");
        return -EINVAL;
    }
    if (!(caps.supported_pixel_formats & PIXEL_FORMAT_MONO01)) {
        LOG_ERR("SSD1306 driver does not support MONO01");
        return -ENOTSUP;
    }

    int err = display_set_pixel_format(display, PIXEL_FORMAT_MONO01);
    if (err) {
        LOG_ERR("Unable to select MONO01 framebuffer: %d", err);
        return err;
    }
    err = display_blanking_off(display);
    if (err) LOG_WRN("Display blanking-off returned %d", err);

    render_baseline();
    return 0;
}

SYS_INIT(hana_raw_dashboard_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
