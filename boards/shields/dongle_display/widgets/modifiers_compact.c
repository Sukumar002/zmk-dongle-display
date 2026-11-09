/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <dt-bindings/zmk/modifiers.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include "modifiers_compact.h"

/*
 * Layout-shift OS mode:
 *   map inactive = macOS semantics
 *   map active   = Windows semantics (GUI <-> Alt)
 *
 * Fall back to the old WIN_BASE layer test if the custom map is absent.
 */
#define HANA_WIN_BASE_LAYER 1

#if DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt_shared))
#define HANA_HAS_GUI_ALT_LAYOUT_SHIFT 1
static const struct device *const hana_gui_alt_layout_shift =
    DEVICE_DT_GET(DT_NODELABEL(layout_shift_map_gui_alt_shared));

struct hana_layout_shift_map_data_view {
    zmk_mod_flags_t modifier_map[8];
    bool active;
};

static bool hana_layout_shift_windows_mode(void) {
    if (!device_is_ready(hana_gui_alt_layout_shift)) {
        return false;
    }

    const struct hana_layout_shift_map_data_view *data =
        hana_gui_alt_layout_shift->data;
    return data->active;
}
#else
#define HANA_HAS_GUI_ALT_LAYOUT_SHIFT 0
#endif

struct modifiers_state {
    uint8_t modifiers;
    bool windows_mode;
};

struct modifier_symbol {
    uint8_t modifier;
    const lv_img_dsc_t *inactive_icon;
    const lv_img_dsc_t *active_icon;
    lv_obj_t *symbol;
};

/* Every visual state is a symbol-only 9x9 bitmap; no text labels are used. */
LV_IMG_DECLARE(modifier_control_off);
LV_IMG_DECLARE(modifier_control_on);
LV_IMG_DECLARE(modifier_shift_off);
LV_IMG_DECLARE(modifier_shift_on);
LV_IMG_DECLARE(modifier_win_off);
LV_IMG_DECLARE(modifier_win_on);
LV_IMG_DECLARE(modifier_cmd_off);
LV_IMG_DECLARE(modifier_cmd_on);
LV_IMG_DECLARE(modifier_opt_off);
LV_IMG_DECLARE(modifier_opt_on);

#define NUM_SYMBOLS 4
#define MODIFIER_BADGE_SIZE 9
#define MODIFIER_BADGE_STEP 11
#define MODIFIER_FAMILY_WIDTH 44

/*
 * Four stable slots. The OS-specific symbol and HID mapping are retained:
 * macOS:    Control, Option, Command, Shift
 * Windows:  Control, Windows, Alt, Shift
 */
static struct modifier_symbol modifier_slots[NUM_SYMBOLS] = {
    {
        .modifier = MOD_LCTL | MOD_RCTL,
        .inactive_icon = &modifier_control_off,
        .active_icon = &modifier_control_on,
    },
    {
        .modifier = MOD_LALT | MOD_RALT,
        .inactive_icon = &modifier_opt_off,
        .active_icon = &modifier_opt_on,
    },
    {
        .modifier = MOD_LGUI | MOD_RGUI,
        .inactive_icon = &modifier_cmd_off,
        .active_icon = &modifier_cmd_on,
    },
    {
        .modifier = MOD_LSFT | MOD_RSFT,
        .inactive_icon = &modifier_shift_off,
        .active_icon = &modifier_shift_on,
    },
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool current_windows_mode;

static void configure_symbol_family(bool windows_mode) {
    if (windows_mode) {
        modifier_slots[0].modifier = MOD_LCTL | MOD_RCTL;
        modifier_slots[0].inactive_icon = &modifier_control_off;
        modifier_slots[0].active_icon = &modifier_control_on;

        modifier_slots[1].modifier = MOD_LGUI | MOD_RGUI;
        modifier_slots[1].inactive_icon = &modifier_win_off;
        modifier_slots[1].active_icon = &modifier_win_on;

        modifier_slots[2].modifier = MOD_LALT | MOD_RALT;
        modifier_slots[2].inactive_icon = &modifier_opt_off;
        modifier_slots[2].active_icon = &modifier_opt_on;
    } else {
        modifier_slots[0].modifier = MOD_LCTL | MOD_RCTL;
        modifier_slots[0].inactive_icon = &modifier_control_off;
        modifier_slots[0].active_icon = &modifier_control_on;

        modifier_slots[1].modifier = MOD_LALT | MOD_RALT;
        modifier_slots[1].inactive_icon = &modifier_opt_off;
        modifier_slots[1].active_icon = &modifier_opt_on;

        modifier_slots[2].modifier = MOD_LGUI | MOD_RGUI;
        modifier_slots[2].inactive_icon = &modifier_cmd_off;
        modifier_slots[2].active_icon = &modifier_cmd_on;
    }

    modifier_slots[3].modifier = MOD_LSFT | MOD_RSFT;
    modifier_slots[3].inactive_icon = &modifier_shift_off;
    modifier_slots[3].active_icon = &modifier_shift_on;
    current_windows_mode = windows_mode;
}

static void set_modifiers(struct modifiers_state state) {
    if (state.windows_mode != current_windows_mode) {
        configure_symbol_family(state.windows_mode);
    }

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        bool active = (state.modifiers & modifier_slots[i].modifier) != 0;
        if (modifier_slots[i].symbol != NULL) {
            lv_img_set_src(
                modifier_slots[i].symbol,
                active ? modifier_slots[i].active_icon
                       : modifier_slots[i].inactive_icon
            );
        }
    }
}

static void modifiers_update_cb(struct modifiers_state state) {
    struct zmk_widget_compact_modifiers *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        ARG_UNUSED(widget);
        set_modifiers(state);
    }
}

static struct modifiers_state modifiers_get_state(const zmk_event_t *eh) {
#if HANA_HAS_GUI_ALT_LAYOUT_SHIFT
    bool windows_mode = hana_layout_shift_windows_mode();
#else
    bool windows_mode = zmk_keymap_layer_active(HANA_WIN_BASE_LAYER);
#endif

    return (struct modifiers_state) {
        .modifiers = zmk_hid_get_explicit_mods(),
        .windows_mode = windows_mode,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(
    widget_modifiers,
    struct modifiers_state,
    modifiers_update_cb,
    modifiers_get_state
)

ZMK_SUBSCRIPTION(widget_modifiers, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(widget_modifiers, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(widget_modifiers, zmk_position_state_changed);

int zmk_widget_compact_modifiers_init(
    struct zmk_widget_compact_modifiers *widget,
    lv_obj_t *parent
) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(
        widget->obj,
        MODIFIER_FAMILY_WIDTH,
        MODIFIER_BADGE_SIZE
    );

    /* The four 9x9 badges sit at x=1,12,23,34 with 2px gaps. */
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        modifier_slots[i].symbol = lv_img_create(widget->obj);
        lv_obj_set_pos(
            modifier_slots[i].symbol,
            1 + (MODIFIER_BADGE_STEP * i),
            0
        );
    }

#if HANA_HAS_GUI_ALT_LAYOUT_SHIFT
    current_windows_mode = hana_layout_shift_windows_mode();
#else
    current_windows_mode = zmk_keymap_layer_active(HANA_WIN_BASE_LAYER);
#endif
    configure_symbol_family(current_windows_mode);

    set_modifiers(modifiers_get_state(NULL));

    sys_slist_append(&widgets, &widget->node);
    widget_modifiers_init();

    return 0;
}

lv_obj_t *zmk_widget_compact_modifiers_obj(
    struct zmk_widget_compact_modifiers *widget
) {
    return widget->obj;
}
