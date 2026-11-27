// SPDX-License-Identifier: GPL-2.0-or-later
//
// Holds a layer on while the pointing device is moving, like ZMK's
// zmk,input-processor-temp-layer -- but with the on/off switch, the timeout
// and the after-a-keystroke guard read from zen_pointer_settings at each
// event, so the Configurator can change them without a rebuild. ZMK's version
// takes all three from devicetree, which is why it could not be used here.
//
// Behaviour matches what the devicetree used to hardcode
// (zip_temp_layer 1 500): pointer motion turns the layer on, it goes off once
// the ball has been still for the timeout, and pressing keys does not by
// itself turn it off. The guard window is the one genuinely new knob, and it
// defaults to 0, which is the old behaviour.

#define DT_DRV_COMPAT zen_input_processor_auto_mouse

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <drivers/input_processor.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>

#include <zephyr/logging/log.h>

#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>

#include "zen_pointer_settings.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* Nothing here unless a board actually wires one of these up. */
#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

/* ...and nothing that touches the keymap unless this build has one.
 *
 * ZMK compiles keymap.c, and the keycode and layer-state events, only for a
 * standalone board or a split central -- so zmk_keymap_layer_activate() and
 * friends do not exist on a peripheral. The comment that used to sit here said
 * the peripheral never instantiates this node, which was true only of the left
 * half without a sensor: input-trackball-* references &zen_auto_mouse, and the
 * same snippet serves both halves, so a left-hand trackball brought the node
 * (and the undefined references) along with it.
 *
 * Holding a layer on is not something a peripheral could do anyway -- the
 * layers live on the central, which is where its reports end up being
 * processed. So the node still becomes a device, to keep the input chain in
 * the overlay resolvable, and that device passes events straight through. */
#define ZEN_HAS_KEYMAP                                                                             \
    (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

#if ZEN_HAS_KEYMAP

struct zen_auto_mouse_config {
    uint8_t layer;
};

struct zen_auto_mouse_data {
    const struct device *dev;
    struct k_mutex lock;
    /* Whether *we* turned the layer on. Something else activating the same
     * layer is not ours to turn off again. */
    bool active;
    /* Uptime at which the layer should come off, extended by every motion
     * event. The work item below chases this rather than being rescheduled on
     * every single report. */
    int64_t off_at;
    int64_t last_key_time;
    struct k_work_delayable work;
};

/* Turning a layer on raises ZMK events, so it does not belong in the input
 * callback. Everything that touches the keymap happens here instead -- the
 * same split ZMK's own temp-layer processor makes. */
static void auto_mouse_work_cb(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct zen_auto_mouse_data *data = CONTAINER_OF(delayable, struct zen_auto_mouse_data, work);
    const struct zen_auto_mouse_config *cfg = data->dev->config;

    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return;
    }

    const int64_t now = k_uptime_get();
    const int64_t remaining = data->off_at - now;

    if (remaining > 0) {
        if (!data->active) {
            data->active = true;
            zmk_keymap_layer_activate(cfg->layer);
            LOG_DBG("auto-mouse: layer %d on", cfg->layer);
        }
        /* Come back when the current deadline runs out; motion since the last
         * wake-up will have pushed it further out. */
        k_work_reschedule(&data->work, K_MSEC(remaining));
    } else if (data->active) {
        data->active = false;
        zmk_keymap_layer_deactivate(cfg->layer);
        LOG_DBG("auto-mouse: layer %d off", cfg->layer);
    }

    k_mutex_unlock(&data->lock);
}

static bool is_motion(const struct input_event *event) {
    if (event->type != INPUT_EV_REL || event->value == 0) {
        return false;
    }

    switch (event->code) {
    case INPUT_REL_X:
    case INPUT_REL_Y:
    case INPUT_REL_WHEEL:
    case INPUT_REL_HWHEEL:
        return true;
    default:
        return false;
    }
}

static int zen_auto_mouse_handle_event(const struct device *dev, struct input_event *event,
                                       uint32_t param1, uint32_t param2,
                                       struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    struct zen_auto_mouse_data *data = dev->data;
    const struct zen_pointer_settings_payload *settings = zen_pointer_settings_payload();

    if (!is_motion(event)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (k_mutex_lock(&data->lock, K_NO_WAIT) < 0) {
        /* The work item has the lock and is about to settle the layer anyway;
         * dropping this one report's contribution to the deadline is better
         * than blocking the input thread. */
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (!settings->auto_mouse_enabled) {
        /* Switched off mid-use: let the work item take the layer back down. */
        if (data->active) {
            data->off_at = 0;
            k_work_reschedule(&data->work, K_NO_WAIT);
        }
        k_mutex_unlock(&data->lock);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    const bool typing = settings->auto_mouse_idle_ms > 0 &&
                        now < data->last_key_time + settings->auto_mouse_idle_ms;

    /* Mid-word: do not switch layers, but do not cut short a layer that is
     * already up either -- the hand is clearly on the ball by then. */
    if (typing && !data->active) {
        k_mutex_unlock(&data->lock);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->off_at = now + settings->auto_mouse_timeout_ms;
    const bool poke = !data->active;

    k_mutex_unlock(&data->lock);

    /* Only when the layer is off. While it is on, the work item re-arms itself
     * against off_at, so there is one timer rather than one per report. */
    if (poke) {
        k_work_reschedule(&data->work, K_NO_WAIT);
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api zen_auto_mouse_driver_api = {
    .handle_event = zen_auto_mouse_handle_event,
};

#define ZEN_AUTO_MOUSE_INST(n)                                                                     \
    static struct zen_auto_mouse_data zen_auto_mouse_data_##n;                                     \
    static const struct zen_auto_mouse_config zen_auto_mouse_config_##n = {                        \
        .layer = DT_INST_PROP(n, layer),                                                           \
    };                                                                                             \
    static int zen_auto_mouse_init_##n(const struct device *dev) {                                 \
        struct zen_auto_mouse_data *data = dev->data;                                              \
        data->dev = dev;                                                                           \
        k_mutex_init(&data->lock);                                                                 \
        k_work_init_delayable(&data->work, auto_mouse_work_cb);                                    \
        return 0;                                                                                  \
    }                                                                                              \
    DEVICE_DT_INST_DEFINE(n, zen_auto_mouse_init_##n, NULL, &zen_auto_mouse_data_##n,              \
                          &zen_auto_mouse_config_##n, POST_KERNEL,                                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &zen_auto_mouse_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ZEN_AUTO_MOUSE_INST)

/* --- Keeping track of what the rest of the keyboard is doing ----------------
 *
 * Two things we have to hear about:
 *   - keystrokes, for the guard window. Subscribed unconditionally, because
 *     unlike ZMK's build-time property this is a setting someone can turn on
 *     at any moment.
 *   - our layer being turned off by something else, so `active` does not go
 *     stale and leave us refusing to turn it on again.
 */

#define ZEN_AUTO_MOUSE_FOR_EACH(fn)                                                                \
    DT_INST_FOREACH_STATUS_OKAY(fn)

#define NOTE_KEYSTROKE(n)                                                                          \
    {                                                                                              \
        struct zen_auto_mouse_data *data = &zen_auto_mouse_data_##n;                               \
        if (k_mutex_lock(&data->lock, K_MSEC(10)) == 0) {                                          \
            data->last_key_time = ev->timestamp;                                                   \
            k_mutex_unlock(&data->lock);                                                           \
        }                                                                                          \
    }

#define NOTE_LAYER_CHANGE(n)                                                                       \
    {                                                                                              \
        struct zen_auto_mouse_data *data = &zen_auto_mouse_data_##n;                               \
        const struct zen_auto_mouse_config *cfg = &zen_auto_mouse_config_##n;                      \
        if (k_mutex_lock(&data->lock, K_MSEC(10)) == 0) {                                          \
            if (data->active && !zmk_keymap_layer_active(cfg->layer)) {                            \
                data->active = false;                                                              \
                data->off_at = 0;                                                                  \
            }                                                                                      \
            k_mutex_unlock(&data->lock);                                                           \
        }                                                                                          \
    }

static int zen_auto_mouse_event_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *key = as_zmk_keycode_state_changed(eh);
    if (key != NULL) {
        if (key->state) {
            const struct zmk_keycode_state_changed *ev = key;
            ZEN_AUTO_MOUSE_FOR_EACH(NOTE_KEYSTROKE)
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (as_zmk_layer_state_changed(eh) != NULL) {
        ZEN_AUTO_MOUSE_FOR_EACH(NOTE_LAYER_CHANGE)
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zen_auto_mouse, zen_auto_mouse_event_listener);
ZMK_SUBSCRIPTION(zen_auto_mouse, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(zen_auto_mouse, zmk_layer_state_changed);

#else /* split peripheral: a device, but one that does nothing */

static int zen_auto_mouse_handle_event(const struct device *dev, struct input_event *event,
                                       uint32_t param1, uint32_t param2,
                                       struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(event);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api zen_auto_mouse_driver_api = {
    .handle_event = zen_auto_mouse_handle_event,
};

#define ZEN_AUTO_MOUSE_STUB(n)                                                                     \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &zen_auto_mouse_driver_api);

DT_INST_FOREACH_STATUS_OKAY(ZEN_AUTO_MOUSE_STUB)

#endif /* ZEN_HAS_KEYMAP */

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
