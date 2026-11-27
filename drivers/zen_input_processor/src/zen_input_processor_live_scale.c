// SPDX-License-Identifier: GPL-2.0-or-later

#define DT_DRV_COMPAT zen_input_processor_live_scale

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/logging/log.h>

#include "zen_pointer_settings.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define ZEN_LIVE_SCALE_CHANNEL_POINTER 0
#define ZEN_LIVE_SCALE_CHANNEL_SCROLL 1

struct zen_live_scale_config {
    uint8_t channel;
};

static int32_t apply_scale(int32_t value, uint16_t scale_percent, int16_t *remainder) {
    int32_t value_scaled = value * (int32_t)scale_percent;

    if (remainder) {
        value_scaled += *remainder;
    }

    int32_t result = value_scaled / 100;

    if (remainder) {
        *remainder = (int16_t)(value_scaled - (result * 100));
    }

    return result;
}

static int zen_live_scale_handle_event(const struct device *dev, struct input_event *event,
                                       uint32_t param1, uint32_t param2,
                                       struct zmk_input_processor_state *state) {
    const struct zen_live_scale_config *cfg = dev->config;

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const struct zen_pointer_settings_payload *settings = zen_pointer_settings_payload();
    int16_t *remainder = (state && state->remainder) ? state->remainder : NULL;

    switch (event->code) {
    case INPUT_REL_X:
        event->value = apply_scale(event->value,
                                   cfg->channel == ZEN_LIVE_SCALE_CHANNEL_SCROLL
                                       ? settings->scroll_scale_percent
                                       : settings->pointer_scale_percent,
                                   remainder);
        if (cfg->channel == ZEN_LIVE_SCALE_CHANNEL_SCROLL ? settings->invert_scroll
                                                           : settings->invert_x) {
            event->value = -event->value;
        }
        break;
    case INPUT_REL_Y:
        event->value = apply_scale(event->value,
                                   cfg->channel == ZEN_LIVE_SCALE_CHANNEL_SCROLL
                                       ? settings->scroll_scale_percent
                                       : settings->pointer_scale_percent,
                                   remainder);
        if (cfg->channel == ZEN_LIVE_SCALE_CHANNEL_SCROLL ? settings->invert_scroll
                                                           : settings->invert_y) {
            event->value = -event->value;
        }
        break;
    case INPUT_REL_WHEEL:
    case INPUT_REL_HWHEEL:
        event->value = apply_scale(event->value, settings->scroll_scale_percent, remainder);
        if (settings->invert_scroll) {
            event->value = -event->value;
        }
        break;
    default:
        break;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api zen_live_scale_driver_api = {
    .handle_event = zen_live_scale_handle_event,
};

#define ZEN_LIVE_SCALE_INST(n)                                                                    \
    static const struct zen_live_scale_config zen_live_scale_config_##n = {                       \
        .channel = DT_INST_PROP(n, channel),                                                       \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &zen_live_scale_config_##n, POST_KERNEL,            \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &zen_live_scale_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ZEN_LIVE_SCALE_INST)
