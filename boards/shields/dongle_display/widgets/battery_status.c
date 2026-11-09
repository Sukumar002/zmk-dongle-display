/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/settings/settings.h>
#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

#ifndef ZMK_SPLIT_BLE_PERIPHERAL_COUNT
#  define ZMK_SPLIT_BLE_PERIPHERAL_COUNT 0
#endif

#define BATTERY_W 18
#define BATTERY_H 8
#define BUFFER_SIZE LV_CANVAS_BUF_SIZE(BATTERY_W, BATTERY_H, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
    bool charging;
    bool present;
};

struct battery_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
    lv_obj_t *charge_icon;
} battery_objects[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET];

static lv_color_t battery_image_buffer[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET][BUFFER_SIZE];
static lv_color_t charge_image_buffer[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET][BUFFER_SIZE];

static uint8_t cached_levels[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET];
static bool cached_charging[ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET];

/*
 * The nice!nano V2 battery source is VDDH, which also sees USB power.
 * Therefore USB-only operation can look like a full battery even when no
 * battery is physically attached. Latch "battery installed" only after the
 * dongle has actually run without USB power; a USB-only dongle can never do
 * that, so its phantom battery stays hidden.
 */
static bool dongle_battery_seen;

#if IS_ENABLED(CONFIG_SETTINGS)
static int dongle_battery_settings_set(const char *name, size_t len,
                                       settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "seen") != 0 || len != sizeof(dongle_battery_seen)) {
        return -ENOENT;
    }

    return read_cb(cb_arg, &dongle_battery_seen, sizeof(dongle_battery_seen));
}

SETTINGS_STATIC_HANDLER_DEFINE(hana_dongle_battery, "hana_dongle_battery", NULL,
                               dongle_battery_settings_set, NULL, NULL);
#endif

static void remember_dongle_battery_present(void) {
    if (dongle_battery_seen) {
        return;
    }

    dongle_battery_seen = true;
#if IS_ENABLED(CONFIG_SETTINGS)
    settings_save_one("hana_dongle_battery/seen", &dongle_battery_seen,
                      sizeof(dongle_battery_seen));
#endif
}

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool charging) {
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    const lv_color_t black = lv_color_black();

    /* Horizontal 10-step battery: 14px body plus a 3px terminal. */
    for (int x = 0; x <= 13; x++) {
        lv_canvas_set_px(canvas, x, 0, black, LV_OPA_COVER);
        lv_canvas_set_px(canvas, x, 7, black, LV_OPA_COVER);
    }
    for (int y = 1; y <= 6; y++) {
        lv_canvas_set_px(canvas, 0, y, black, LV_OPA_COVER);
        lv_canvas_set_px(canvas, 13, y, black, LV_OPA_COVER);
    }
    for (int y = 3; y <= 4; y++) {
        for (int x = 14; x <= 16; x++) {
            lv_canvas_set_px(canvas, x, y, black, LV_OPA_COVER);
        }
    }

    if (charging) {
        /* Small lightning mark inside the body while charging. */
        const uint8_t bolt[][2] = {{7,1},{6,2},{7,2},{5,3},{6,3},{7,3},{6,4},{5,5},{6,5}};
        for (size_t i = 0; i < ARRAY_SIZE(bolt); i++) {
            lv_canvas_set_px(canvas, bolt[i][0], bolt[i][1], black, LV_OPA_COVER);
        }
    } else {
        uint8_t steps = (uint8_t)((MIN(level, 100) + 9) / 10);
        for (int x = 2; x < 2 + steps && x <= 11; x++) {
            for (int y = 2; y <= 5; y++) {
                lv_canvas_set_px(canvas, x, y, black, LV_OPA_COVER);
            }
        }
    }
}


static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    ARG_UNUSED(widget);
    if (state.source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET) {
        return;
    }

    cached_levels[state.source] = state.level;
    cached_charging[state.source] = state.charging;

    /* The compact dashboard intentionally shows the two halves only.
     * Keep dongle battery events cached, but don't consume a visible row. */
    if (SOURCE_OFFSET && state.source == 0) {
        return;
    }

    const uint8_t half = state.source - SOURCE_OFFSET;
    if (half >= 2) {
        return;
    }

    lv_obj_t *symbol = battery_objects[state.source].symbol;
    lv_obj_t *label = battery_objects[state.source].label;
    lv_obj_t *charge_icon = battery_objects[state.source].charge_icon;

    if (!state.present || state.level == 0xff) {
        char side = half == 0 ? 'L' : 'R';
        lv_label_set_text_fmt(label, "%c --", side);
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(symbol, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char side = half == 0 ? 'L' : 'R';
    if (state.level >= 100) {
        lv_label_set_text_fmt(label, "%c100", side);
    } else {
        lv_label_set_text_fmt(label, "%c %02u", side, state.level);
    }

    draw_battery(symbol, state.level, state.charging);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    uint8_t source = ev->source + SOURCE_OFFSET;
    return (struct battery_state){
        .source = source,
        .level = ev->state_of_charge,
        .charging = cached_charging[source],
        .present = true,
    };
}

static struct battery_state peripheral_charge_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_charge_state_changed *ev =
        as_zmk_peripheral_charge_state_changed(eh);
    uint8_t source = ev->source + SOURCE_OFFSET;
    return (struct battery_state){
        .source = source,
        .level = cached_levels[source],
        .charging = ev->charging,
        .present = true,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    uint8_t level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge();

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool usb_present = zmk_usb_is_powered();
#else
    bool usb_present = false;
#endif

    /*
     * If the dongle is alive without USB and reports a battery level, a real
     * battery must be installed. Remember that fact for later USB charging
     * sessions so the battery field can remain visible while plugged in.
     */
    if (!usb_present && level > 0) {
        remember_dongle_battery_present();
    }

    return (struct battery_state) {
        .source = 0,
        .level = level,
        .usb_present = usb_present,
        .present = dongle_battery_seen,
        /*
         * Until the exact dongle clone charge-status GPIO is known, VBUS plus
         * a not-full battery is the charging indication for an installed pack.
         */
        .charging = dongle_battery_seen && usb_present && level < 100,
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) {
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else if (as_zmk_peripheral_charge_state_changed(eh) != NULL) {
        return peripheral_charge_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_charge_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    /* Compact 128x32 dashboard: fixed coordinates match the original tested
     * L/R battery rows, while USB and BT retain their existing positions. */
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 128, 32);
    lv_obj_set_pos(widget->obj, 0, 0);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(widget->obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(widget->obj, 0, LV_PART_MAIN);

    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
        lv_obj_t *battery_label = lv_label_create(widget->obj);
        lv_obj_t *charge_canvas = lv_canvas_create(widget->obj);

        lv_canvas_set_buffer(image_canvas, battery_image_buffer[i], BATTERY_W, BATTERY_H, LV_COLOR_FORMAT_L8);
        lv_canvas_set_buffer(charge_canvas, charge_image_buffer[i], BATTERY_W, BATTERY_H, LV_COLOR_FORMAT_L8);

        if (SOURCE_OFFSET && i == 0) {
            /* Dongle battery is tracked but not shown in the two-row dashboard. */
            lv_obj_set_pos(image_canvas, -24, -8);
            lv_obj_set_pos(charge_canvas, -24, -8);
            lv_obj_set_pos(battery_label, -24, -8);
        } else {
            const int half = i - SOURCE_OFFSET;
            const int y = half == 0 ? 3 : 19;
            lv_obj_set_pos(battery_label, 79, y);
            lv_obj_set_pos(image_canvas, 108, y);
            lv_obj_set_pos(charge_canvas, 108, y);
        }

        lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charge_canvas, LV_OBJ_FLAG_HIDDEN);

        battery_objects[i] = (struct battery_object) {
            .symbol = image_canvas,
            .label = battery_label,
            .charge_icon = charge_canvas,
        };
    }

    sys_slist_append(&widgets, &widget->node);
    widget_dongle_battery_status_init();
    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
