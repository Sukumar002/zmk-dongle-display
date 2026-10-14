#include <stdbool.h>
#include <string.h>

#include <zephyr/settings/settings.h>

#include "hana_base_view.h"
#include "../boards/shields/dongle_display/widgets/bongo_cat.h"
#include "../boards/shields/dongle_display/widgets/wpm_status.h"
#include "../boards/shields/dongle_display/widgets/layer_status.h"

static bool cat_enabled = true;

bool hana_base_view_cat_enabled(void) {
    return cat_enabled;
}

static void apply_base_view(void) {
    zmk_widget_bongo_cat_set_base_enabled(cat_enabled);
    zmk_widget_wpm_set_base_enabled(cat_enabled);
    zmk_widget_layer_status_set_base_text_enabled(!cat_enabled);
}

void hana_base_view_toggle(void) {
    cat_enabled = !cat_enabled;
#if IS_ENABLED(CONFIG_SETTINGS)
    settings_save_one("hana_base_view/cat", &cat_enabled, sizeof(cat_enabled));
#endif
    apply_base_view();
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int base_view_settings_set(const char *name, size_t len,
                                  settings_read_cb read_cb, void *cb_arg) {
    if (strcmp(name, "cat") != 0 || len != sizeof(cat_enabled)) {
        return -ENOENT;
    }

    int rc = read_cb(cb_arg, &cat_enabled, sizeof(cat_enabled));
    if (rc < 0) {
        return rc;
    }

    apply_base_view();
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(hana_base_view, "hana_base_view", NULL,
                               base_view_settings_set, NULL, NULL);
#endif
