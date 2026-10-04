#pragma once

#include <stdbool.h>
#include <zmk/event_manager.h>

struct hana_charge_state_changed {
    bool charging;
};

ZMK_EVENT_DECLARE(hana_charge_state_changed);
