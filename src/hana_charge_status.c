#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "hana_charge_state_changed.h"

LOG_MODULE_REGISTER(hana_charge_status, LOG_LEVEL_INF);

/*
 * Seeed XIAO nRF52840 charging status:
 * P0.17 LOW  = actively charging
 * P0.17 HIGH = not charging / charge complete
 */
#define CHARGE_GPIO_NODE DT_NODELABEL(gpio0)
#define CHARGE_GPIO_PIN 17

static const struct device *charge_gpio =
    DEVICE_DT_GET(CHARGE_GPIO_NODE);

static struct gpio_callback charge_gpio_cb;

static bool charging;
static bool charging_state_initialized;

static void update_charging_state(void) {
    int value = gpio_pin_get(charge_gpio, CHARGE_GPIO_PIN);

    if (value < 0) {
        LOG_ERR("Failed to read charge-status GPIO: %d", value);
        return;
    }

    bool new_charging = (value == 0);

    /*
     * Raise the event once at startup, then only when the charging
     * state actually changes.
     */
    if (!charging_state_initialized || new_charging != charging) {
        charging = new_charging;
        charging_state_initialized = true;

        LOG_INF("Hana charging state: %s",
                charging ? "charging" : "not charging");

        raise_hana_charge_state_changed(
            (struct hana_charge_state_changed) {
                .charging = charging,
            }
        );
    }
}

static void charge_gpio_changed(const struct device *dev,
                                struct gpio_callback *cb,
                                uint32_t pins) {
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    update_charging_state();
}

static int hana_charge_status_init(void) {
    int err;

    if (!device_is_ready(charge_gpio)) {
        LOG_ERR("GPIO0 device is not ready");
        return -ENODEV;
    }

    err = gpio_pin_configure(charge_gpio,
                             CHARGE_GPIO_PIN,
                             GPIO_INPUT);

    if (err) {
        LOG_ERR("Failed to configure charge-status GPIO: %d", err);
        return err;
    }

    gpio_init_callback(&charge_gpio_cb,
                       charge_gpio_changed,
                       BIT(CHARGE_GPIO_PIN));

    err = gpio_add_callback(charge_gpio, &charge_gpio_cb);

    if (err) {
        LOG_ERR("Failed to add charge-status GPIO callback: %d", err);
        return err;
    }

    err = gpio_pin_interrupt_configure(
        charge_gpio,
        CHARGE_GPIO_PIN,
        GPIO_INT_EDGE_BOTH
    );

    if (err) {
        LOG_ERR("Failed to configure charge-status interrupt: %d", err);
        return err;
    }

    update_charging_state();

    return 0;
}

SYS_INIT(hana_charge_status_init,
         APPLICATION,
         CONFIG_APPLICATION_INIT_PRIORITY);
