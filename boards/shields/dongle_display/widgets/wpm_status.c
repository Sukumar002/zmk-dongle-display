#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

#include "wpm_status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool base_wpm_enabled = true;
static int last_wpm = 0;
static int64_t last_wpm_update_time = 0;

#define WPM_UPDATE_INTERVAL_MS 250

struct wpm_status_state {
    int wpm;
};

static struct wpm_status_state get_state(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);

    return (struct wpm_status_state) {
        .wpm = ev ? ev->state : last_wpm,
    };
}

static void set_wpm(struct zmk_widget_wpm_status *widget,
                    struct wpm_status_state state) {
    /*
     * WPM belongs to BASE together with Bongo Cat. Keep this guard here
     * in addition to the layer listener so a late WPM event cannot make the
     * widget visible on NUM/NAV/SET/MOUSE.
     */
    if (zmk_keymap_highest_layer_active() != 0 || !base_wpm_enabled) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);

    int64_t now = k_uptime_get();
    if (state.wpm == last_wpm &&
        (now - last_wpm_update_time) < WPM_UPDATE_INTERVAL_MS) {
        return;
    }

    last_wpm = state.wpm;
    last_wpm_update_time = now;

    char wpm_text[12];
    snprintf(wpm_text, sizeof(wpm_text), "%d WPM", state.wpm);
    lv_label_set_text(widget->wpm_label, wpm_text);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_wpm(widget, state);
    }
}

static int wpm_layer_listener(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    const bool base = zmk_keymap_highest_layer_active() == 0;

    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (base && base_wpm_enabled) {
            char wpm_text[12];
            snprintf(wpm_text, sizeof(wpm_text), "%d WPM", last_wpm);
            lv_label_set_text(widget->wpm_label, wpm_text);
            lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_wpm_layer, wpm_layer_listener);
ZMK_SUBSCRIPTION(widget_wpm_layer, zmk_layer_state_changed);

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget,
                               lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 50, 26);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(widget->obj, 1, 0);
    lv_obj_set_style_border_color(widget->obj, lv_color_black(), 0);
    lv_obj_set_style_radius(widget->obj, 2, 0);
    lv_obj_set_style_pad_all(widget->obj, 0, 0);

    widget->wpm_label = lv_label_create(widget->obj);
    lv_obj_set_width(widget->wpm_label, 48);
    lv_label_set_long_mode(widget->wpm_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(widget->wpm_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(widget->wpm_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(widget->wpm_label, "0 WPM");

    if (zmk_keymap_highest_layer_active() != 0 || !base_wpm_enabled) {
        lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    }

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    return 0;
}

void zmk_widget_wpm_set_base_enabled(bool enabled) {
    base_wpm_enabled = enabled;

    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (enabled && zmk_keymap_highest_layer_active() == 0) {
            char wpm_text[12];
            snprintf(wpm_text, sizeof(wpm_text), "%d WPM", last_wpm);
            lv_label_set_text(widget->wpm_label, wpm_text);
            lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget) {
    return widget->obj;
}
