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

#define BUFFER_SIZE LV_CANVAS_BUF_SIZE(7, 8, LV_COLOR_FORMAT_GET_BPP(LV_COLOR_FORMAT_L8), LV_DRAW_BUF_STRIDE_ALIGN)

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

static void draw_battery(lv_obj_t *canvas, uint8_t level, bool usb_present) {
    ARG_UNUSED(usb_present);
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);

    /*
     * Draw a dedicated 7x8 battery glyph. Keep it independent from the
     * keyboard/timeout canvas so battery status can never inherit that icon.
     */
    const lv_color_t white = lv_color_white();

    /* Battery cap and outer case. */
    lv_canvas_set_px(canvas, 3, 0, white, LV_OPA_COVER);
    for (int x = 1; x <= 5; x++) {
        lv_canvas_set_px(canvas, x, 1, white, LV_OPA_COVER);
        lv_canvas_set_px(canvas, x, 7, white, LV_OPA_COVER);
    }
    for (int y = 2; y <= 6; y++) {
        lv_canvas_set_px(canvas, 1, y, white, LV_OPA_COVER);
        lv_canvas_set_px(canvas, 5, y, white, LV_OPA_COVER);
    }

    /* Fill one to five interior rows according to the reported charge. */
    uint8_t rows = level == 0 ? 0 : (uint8_t)((level + 19) / 20);
    if (rows > 5) {
        rows = 5;
    }
    for (int y = 6; y >= 7 - rows; y--) {
        for (int x = 2; x <= 4; x++) {
            lv_canvas_set_px(canvas, x, y, white, LV_OPA_COVER);
        }
    }
}

static void draw_charge_flash(lv_obj_t *canvas) {
    lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_TRANSP);

    const lv_color_t white = lv_color_white();
    const uint8_t pixels[][2] = {
        {3,0}, {2,1}, {3,1}, {1,2}, {2,2},
        {1,3}, {2,3}, {3,3}, {2,4}, {3,4},
        {2,5}, {1,6}, {1,7}
    };

    for (size_t i = 0; i < ARRAY_SIZE(pixels); i++) {
        lv_canvas_set_px(canvas, pixels[i][0], pixels[i][1], white, LV_OPA_COVER);
    }
}

static void charge_opa_anim_cb(void *obj, int32_t value) {
    lv_obj_set_style_opa((lv_obj_t *)obj, value, LV_PART_MAIN);
}

static void start_charge_breathing(lv_obj_t *icon) {
    lv_anim_del(icon, charge_opa_anim_cb);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, icon);
    lv_anim_set_exec_cb(&anim, charge_opa_anim_cb);
    lv_anim_set_values(&anim, LV_OPA_30, LV_OPA_COVER);
    lv_anim_set_duration(&anim, 800);
    lv_anim_set_playback_duration(&anim, 800);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&anim);
}

static void stop_charge_breathing(lv_obj_t *icon) {
    lv_anim_del(icon, charge_opa_anim_cb);
    lv_obj_set_style_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET) {
        return;
    }

    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    lv_obj_t *symbol = battery_objects[state.source].symbol;
    lv_obj_t *label = battery_objects[state.source].label;
    lv_obj_t *charge_icon = battery_objects[state.source].charge_icon;

    cached_levels[state.source] = state.level;
    cached_charging[state.source] = state.charging;

    const bool is_dongle = SOURCE_OFFSET && state.source == 0;

    /*
     * With no battery attached, the dongle ADC can report an empty value while
     * USB is powering the board. Keep the new dongle battery field out of the
     * way in that legacy USB-only case.
     */
    if (is_dongle && !state.present) {
        stop_charge_breathing(charge_icon);
        lv_obj_add_flag(symbol, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (is_dongle && state.level == 0) {
        stop_charge_breathing(charge_icon);
        lv_obj_add_flag(symbol, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    const bool full = state.level >= 100;
    draw_battery(symbol, full ? 100 : state.level, false);
    lv_label_set_text_fmt(label, "%u%%", state.level);

    if (state.level > 0) {
        lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(label);

        if (state.charging && !full) {
            /*
             * Charging replaces the normal battery glyph with the breathing
             * lightning glyph. The percentage remains visible beside it.
             */
            lv_obj_add_flag(symbol, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(charge_icon);
            start_charge_breathing(charge_icon);
        } else {
            /*
             * Normal and fully-charged states use the battery glyph.
             * A full battery is drawn at 100%, with no charging flash.
             */
            stop_charge_breathing(charge_icon);
            lv_obj_add_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(symbol);
        }
    } else {
        stop_charge_breathing(charge_icon);
        lv_obj_add_flag(symbol, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charge_icon, LV_OBJ_FLAG_HIDDEN);
    }
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
    widget->obj = lv_obj_create(parent);

    /*
     * Fixed battery layout:
     *   - dongle field begins near the centre of the OLED
     *   - left/right half percentages stay in the original far-right stack
     *
     * Explicit coordinates avoid the clipping/regression introduced by
     * nesting the half labels around the new dongle field.
     */
    lv_obj_set_size(widget->obj, 78, 24);

    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
        lv_obj_t *battery_label = lv_label_create(widget->obj);
        lv_obj_t *charge_canvas = lv_canvas_create(widget->obj);

        lv_canvas_set_buffer(image_canvas, battery_image_buffer[i], 7, 8, LV_COLOR_FORMAT_L8);
        lv_canvas_set_buffer(charge_canvas, charge_image_buffer[i], 7, 8, LV_COLOR_FORMAT_L8);
        draw_charge_flash(charge_canvas);

        if (SOURCE_OFFSET && i == 0) {
            /* Dongle: centre-side field. */
            lv_obj_set_pos(image_canvas, 0, 4);
            lv_obj_set_pos(charge_canvas, 0, 4);
            lv_obj_set_pos(battery_label, 9, 2);
        } else {
            /* Split halves: preserve a stable right-side percentage stack. */
            int peripheral_index = i - SOURCE_OFFSET;
            int y = peripheral_index * 10;
            lv_obj_set_pos(battery_label, 43, y - 1);
            lv_obj_set_pos(image_canvas, 70, y);
            lv_obj_set_pos(charge_canvas, 70, y);
        }

        lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(charge_canvas, LV_OBJ_FLAG_HIDDEN);

        battery_objects[i] = (struct battery_object){
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
