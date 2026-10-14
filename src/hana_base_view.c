#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/settings/settings.h>

#include "hana_base_view.h"
#include "../boards/shields/dongle_display/widgets/bongo_cat.h"
#include "../boards/shields/dongle_display/widgets/wpm_status.h"
#include "../boards/shields/dongle_display/widgets/layer_status.h"

static uint8_t base_view_mode = HANA_BASE_VIEW_CAT_WPM;

enum hana_base_view_mode hana_base_view_get_mode(void) {
    return (enum hana_base_view_mode)base_view_mode;
}

static void apply_base_view(void) {
    bool cat = base_view_mode == HANA_BASE_VIEW_CAT_WPM ||
               base_view_mode == HANA_BASE_VIEW_CAT_ONLY;
    bool text = base_view_mode == HANA_BASE_VIEW_TEXT_WPM ||
                base_view_mode == HANA_BASE_VIEW_TEXT_ONLY;
    bool wpm = base_view_mode == HANA_BASE_VIEW_CAT_WPM ||
               base_view_mode == HANA_BASE_VIEW_TEXT_WPM;

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    zmk_widget_bongo_cat_set_base_enabled(cat);
#else
    ARG_UNUSED(cat);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_WPM)
    zmk_widget_wpm_set_base_enabled(wpm);
#else
    ARG_UNUSED(wpm);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_LAYER)
    zmk_widget_layer_status_set_base_text_enabled(text);
#else
    ARG_UNUSED(text);
#endif
}

void hana_base_view_cycle(void) {
    base_view_mode = (base_view_mode + 1) % HANA_BASE_VIEW_MODE_COUNT;

#if IS_ENABLED(CONFIG_SETTINGS)
    settings_save_one("hana_base_view/mode", &base_view_mode, sizeof(base_view_mode));
#endif

    apply_base_view();
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int base_view_settings_set(const char *name, size_t len,
                                  settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "mode") == 0) {
        uint8_t mode;

        if (len != sizeof(mode)) {
            return -EINVAL;
        }

        int rc = read_cb(cb_arg, &mode, sizeof(mode));
        if (rc < 0) {
            return rc;
        }

        if (mode >= HANA_BASE_VIEW_MODE_COUNT) {
            return -EINVAL;
        }

        base_view_mode = mode;
        apply_base_view();
        return 0;
    }

    if (strcmp(name, "cat") == 0) {
        bool cat;

        if (len != sizeof(cat)) {
            return -EINVAL;
        }

        int rc = read_cb(cb_arg, &cat, sizeof(cat));
        if (rc < 0) {
            return rc;
        }

        base_view_mode = cat ? HANA_BASE_VIEW_CAT_WPM : HANA_BASE_VIEW_TEXT_ONLY;
        apply_base_view();
        return 0;
    }

    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(hana_base_view, "hana_base_view", NULL,
                               base_view_settings_set, NULL, NULL);
#endif
