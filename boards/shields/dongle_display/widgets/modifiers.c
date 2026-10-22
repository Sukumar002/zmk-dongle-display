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

#include "modifiers.h"

/*
 * Layout-shift OS mode for Sweep:
 *   map inactive = Windows semantics (default)
 *   map active   = macOS semantics (GUI <-> Alt)
 *
 * The Sweep keymap defines Windows as the default and toggles the map ON
 * for Mac. Keep the display mode aligned with that behavior.
 */
#define HANA_WIN_BASE_LAYER 1

#if DT_NODE_EXISTS(DT_NODELABEL(layout_shift_map_gui_alt))
#define HANA_HAS_GUI_ALT_LAYOUT_SHIFT 1
static const struct device *const hana_gui_alt_layout_shift =
    DEVICE_DT_GET(DT_NODELABEL(layout_shift_map_gui_alt));

struct hana_layout_shift_map_data_view {
    zmk_mod_flags_t modifier_map[8];
    bool active;
};

static bool hana_layout_shift_windows_mode(void) {
    if (!device_is_ready(hana_gui_alt_layout_shift)) {
        /* Sweep's safe/default mode is Windows until the map is ready. */
        return true;
    }

    const struct hana_layout_shift_map_data_view *data =
        hana_gui_alt_layout_shift->data;
    return !data->active;
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
    const lv_img_dsc_t *symbol_dsc;
    lv_obj_t *symbol;
    lv_obj_t *selection_line;
    bool is_active;
};

/* All six icon assets already exist in modifiers_sym.c. */
LV_IMG_DECLARE(control_icon);
LV_IMG_DECLARE(shift_icon);
LV_IMG_DECLARE(opt_icon);
LV_IMG_DECLARE(cmd_icon);
LV_IMG_DECLARE(alt_icon);
LV_IMG_DECLARE(win_icon);

/*
 * Four physical display slots.
 *
 * MAC:
 *   Ctrl | Option | Command | Shift
 *
 * WINDOWS:
 *   Win | Alt | Ctrl | Shift
 */
static struct modifier_symbol modifier_slots[4] = {
    {
        .modifier = MOD_LCTL | MOD_RCTL,
        .symbol_dsc = &control_icon,
    },
    {
        .modifier = MOD_LALT | MOD_RALT,
        .symbol_dsc = &opt_icon,
    },
    {
        .modifier = MOD_LGUI | MOD_RGUI,
        .symbol_dsc = &cmd_icon,
    },
    {
        .modifier = MOD_LSFT | MOD_RSFT,
        .symbol_dsc = &shift_icon,
    },
};

#define NUM_SYMBOLS 4

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

static bool current_windows_mode = false;

static void anim_y_cb(void *var, int32_t v) {
    lv_obj_set_y(var, v);
}

static void move_object_y(void *obj, int32_t from, int32_t to) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_duration(&a, 200);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_set_values(&a, from, to);
    lv_anim_start(&a);
}

static void configure_mac_symbols(void) {
    /* Ctrl */
    modifier_slots[0].modifier = MOD_LCTL | MOD_RCTL;
    modifier_slots[0].symbol_dsc = &control_icon;

    /* Option */
    modifier_slots[1].modifier = MOD_LALT | MOD_RALT;
    modifier_slots[1].symbol_dsc = &opt_icon;

    /* Command */
    modifier_slots[2].modifier = MOD_LGUI | MOD_RGUI;
    modifier_slots[2].symbol_dsc = &cmd_icon;

    /* Shift */
    modifier_slots[3].modifier = MOD_LSFT | MOD_RSFT;
    modifier_slots[3].symbol_dsc = &shift_icon;
}

static void configure_windows_symbols(void) {
    /* Ctrl */
    modifier_slots[0].modifier = MOD_LCTL | MOD_RCTL;
    modifier_slots[0].symbol_dsc = &control_icon;

    /* Windows */
    modifier_slots[1].modifier = MOD_LGUI | MOD_RGUI;
    modifier_slots[1].symbol_dsc = &win_icon;

    /* Alt */
    modifier_slots[2].modifier = MOD_LALT | MOD_RALT;
    modifier_slots[2].symbol_dsc = &alt_icon;

    /* Shift */
    modifier_slots[3].modifier = MOD_LSFT | MOD_RSFT;
    modifier_slots[3].symbol_dsc = &shift_icon;
}


static void update_symbol_family(bool windows_mode) {
    if (windows_mode == current_windows_mode) {
        return;
    }

    if (windows_mode) {
        configure_windows_symbols();
    } else {
        configure_mac_symbols();
    }

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        if (modifier_slots[i].symbol != NULL) {
            lv_img_set_src(modifier_slots[i].symbol,
                           modifier_slots[i].symbol_dsc);
        }

        /*
         * Force modifier activity to be re-evaluated after the layout
         * changes. This avoids carrying an underline state from a
         * different modifier occupying the same display slot.
         */
        modifier_slots[i].is_active = false;
    }

    current_windows_mode = windows_mode;
}

static void set_modifiers(lv_obj_t *widget, struct modifiers_state state) {
    update_symbol_family(state.windows_mode);

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        bool mod_is_active =
            state.modifiers & modifier_slots[i].modifier;

        if (mod_is_active && !modifier_slots[i].is_active) {
            move_object_y(modifier_slots[i].symbol, 1, 0);
            move_object_y(modifier_slots[i].selection_line,
                          SIZE_SYMBOLS + 4,
                          SIZE_SYMBOLS + 2);

            modifier_slots[i].is_active = true;

        } else if (!mod_is_active && modifier_slots[i].is_active) {
            move_object_y(modifier_slots[i].symbol, 0, 1);
            move_object_y(modifier_slots[i].selection_line,
                          SIZE_SYMBOLS + 2,
                          SIZE_SYMBOLS + 4);

            modifier_slots[i].is_active = false;
        }
    }
}

static void modifiers_update_cb(struct modifiers_state state) {
    struct zmk_widget_modifiers *widget;

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_modifiers(widget->obj, state);
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

int zmk_widget_modifiers_init(
    struct zmk_widget_modifiers *widget,
    lv_obj_t *parent
) {
    widget->obj = lv_obj_create(parent);

    lv_obj_set_size(
        widget->obj,
        NUM_SYMBOLS * (SIZE_SYMBOLS + 1) + 1,
        SIZE_SYMBOLS + 3
    );

    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 2);

    static const lv_point_precise_t selection_line_points[] = {
        {0, 0},
        {SIZE_SYMBOLS, 0}
    };

    /*
     * Start with the icon family matching the current OS mode.
     */
#if HANA_HAS_GUI_ALT_LAYOUT_SHIFT
    current_windows_mode = hana_layout_shift_windows_mode();
#else
    current_windows_mode = zmk_keymap_layer_active(HANA_WIN_BASE_LAYER);
#endif
    if (current_windows_mode) {
        configure_windows_symbols();
    } else {
        configure_mac_symbols();
    }

    for (int i = 0; i < NUM_SYMBOLS; i++) {
        modifier_slots[i].symbol = lv_img_create(widget->obj);

        lv_obj_align(
            modifier_slots[i].symbol,
            LV_ALIGN_TOP_LEFT,
            1 + (SIZE_SYMBOLS + 1) * i,
            1
        );

        lv_img_set_src(
            modifier_slots[i].symbol,
            modifier_slots[i].symbol_dsc
        );

        modifier_slots[i].selection_line =
            lv_line_create(widget->obj);

        lv_line_set_points(
            modifier_slots[i].selection_line,
            selection_line_points,
            2
        );

        lv_obj_add_style(
            modifier_slots[i].selection_line,
            &style_line,
            0
        );

        lv_obj_align_to(
            modifier_slots[i].selection_line,
            modifier_slots[i].symbol,
            LV_ALIGN_OUT_BOTTOM_LEFT,
            0,
            3
        );
    }

    sys_slist_append(&widgets, &widget->node);

    widget_modifiers_init();

    return 0;
}

lv_obj_t *zmk_widget_modifiers_obj(
    struct zmk_widget_modifiers *widget
) {
    return widget->obj;
}
