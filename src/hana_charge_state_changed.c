#include <zmk/event_manager.h>
#include <zmk/split/transport/peripheral.h>
#include <zmk/split/transport/types.h>

#include "hana_charge_state_changed.h"

ZMK_EVENT_IMPL(hana_charge_state_changed);

static int hana_charge_state_listener(const zmk_event_t *eh) {
    const struct hana_charge_state_changed *ev = as_hana_charge_state_changed(eh);

    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    struct zmk_split_transport_peripheral_event split_ev = {
        .type = ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_CHARGE_STATUS_EVENT,
        .data = {
            .charge_status_event = {
                .charging = ev->charging ? 1 : 0,
            },
        },
    };

    zmk_split_peripheral_report_event(&split_ev);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(hana_charge_state_transport, hana_charge_state_listener);
ZMK_SUBSCRIPTION(hana_charge_state_transport, hana_charge_state_changed);
