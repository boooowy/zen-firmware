/*
 * ZEN 160x68 Sharp Memory LCD status screen.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/display.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>
#include <zmk/split/transport/central.h>
#include <zmk/split/transport/types.h>

LV_FONT_DECLARE(zen_font_unscii_8x16);
LV_FONT_DECLARE(zen_font_dot_status);
LV_FONT_DECLARE(zen_font_dot_ui);
LV_FONT_DECLARE(zen_font_dot_sleep_small);
LV_FONT_DECLARE(zen_font_dot_sleep_medium);
LV_FONT_DECLARE(zen_font_dot_sleep_large);
LV_FONT_DECLARE(zen_font_dot_12);
LV_FONT_DECLARE(zen_font_dot_14);
LV_FONT_DECLARE(zen_font_dot_16);
LV_FONT_DECLARE(zen_font_dot_28);

extern const struct zmk_split_transport_central *active_transport;

static lv_obj_t *logo_label;
static lv_obj_t *logo_subtitle;
static lv_obj_t *battery_label;
static lv_obj_t *connection_label;
static lv_obj_t *layer_label;
static lv_obj_t *auto_mouse_label;
static lv_obj_t *key_input_label;
static lv_obj_t *notification_label;
static lv_obj_t *sleep_z_small_label;
static lv_obj_t *sleep_z_large_label;
static lv_obj_t *status_screen_root;

static uint8_t boot_tick;
static bool boot_complete;
static bool dashboard_visible;
static bool key_input_visible;
static bool notification_visible;
static bool screen_blank;
static bool sleep_visible;
static volatile bool right_battery_known;
static volatile uint8_t right_battery_level;

#define KEY_HOLD_MS 1000
#define KEY_FADE_MS 1000
#define KEY_VISIBLE_MS (KEY_HOLD_MS + KEY_FADE_MS)
#define STATUS_RETURN_MS 2500
#define SCREEN_IDLE_MS (5 * 60 * 1000)
/* Draw the retained sleep image before ZMK's matching idle timeout stops display updates. */
#define SLEEP_SCREEN_MS (SCREEN_IDLE_MS - 10000)
#define NOTIFICATION_MS 2000
#define STARTUP_NOTIFICATION_SUPPRESS_MS 5000
#define TEXT_SHUFFLE_FRAMES 6
#define LOW_BATTERY_PERCENT 10
/* Sharp specifies refreshing static image data within two hours. */
#define PANEL_STATIC_REFRESH_MS (60 * 60 * 1000)
#define LCD_WIDTH 160
#define LCD_HEIGHT 68
#define LCD_POWER_PIN 8
#define LCD_EXTCOMIN_PIN 19

static struct k_spinlock key_input_lock;
static char pending_key_text[32];
static char displayed_key_text[32];
static volatile int64_t last_key_press_ms;
static uint8_t active_modifiers;
static lv_opa_t last_key_opacity = LV_OPA_COVER;

static bool notification_state_initialized;
static int64_t notification_until_ms;
static int64_t connection_notifications_after_ms;
static char notification_text[32];
static char displayed_notification_text[32];
static zmk_keymap_layer_index_t previous_layer;
static uint8_t previous_ble_profile;
static enum zmk_transport previous_transport;
static bool previous_split_connected;
static bool previous_host_connected;
static bool previous_left_battery_low;
static bool previous_right_battery_low;

#if !defined(CONFIG_ZEN_LCD_DOT_STATUS)
struct status_lamp {
    bool initialized;
    bool connected;
    uint8_t animation_ticks;
};

static struct status_lamp split_lamp;
static struct status_lamp host_lamp;
#endif

static char last_battery_text[40];
static char last_connection_text[40];
static char last_layer_text[40];
static char last_auto_mouse_text[40];
static uint8_t battery_shuffle_frame;
static uint8_t connection_shuffle_frame;
static uint8_t layer_shuffle_frame;
static uint8_t auto_mouse_shuffle_frame;
static uint8_t notification_shuffle_frame;
static uint32_t shuffle_rng_state = 0x5a454e31;

static void panel_static_refresh_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    if (status_screen_root == NULL) {
        return;
    }

    /* Execute on ZMK's display queue because LVGL is not thread-safe. */
    lv_obj_invalidate(status_screen_root);
    lv_refr_now(lv_disp_get_default());
}

K_WORK_DEFINE(panel_static_refresh_work, panel_static_refresh_work_cb);

static void panel_static_refresh_timer_cb(struct k_timer *timer) {
    ARG_UNUSED(timer);

    if (zmk_display_is_initialized()) {
        k_work_submit_to_queue(zmk_display_work_q(), &panel_static_refresh_work);
    }
}

K_TIMER_DEFINE(panel_static_refresh_timer, panel_static_refresh_timer_cb, NULL);

/*
 * ZMK enters system-off after its long idle timeout. EXTCOMIN stops in that
 * state, so the panel must not remain powered. Clear the retained image and
 * remove LCD power before the activity handler calls sys_poweroff().
 */
static int zen_lcd_power_event_handler(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *event = as_zmk_activity_state_changed(eh);

    if (event == NULL || event->state != ZMK_ACTIVITY_SLEEP) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    const struct device *gpio = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    static uint8_t white_frame[LCD_WIDTH * LCD_HEIGHT / 8];

    if (device_is_ready(display)) {
        memset(white_frame, 0xff, sizeof(white_frame));
        const struct display_buffer_descriptor descriptor = {
            .buf_size = sizeof(white_frame),
            .width = LCD_WIDTH,
            .height = LCD_HEIGHT,
            .pitch = LCD_WIDTH,
        };
        display_write(display, 0, 0, &descriptor, white_frame);
    }

    if (device_is_ready(gpio)) {
        gpio_pin_set(gpio, LCD_EXTCOMIN_PIN, 0);
        k_busy_wait(30);
        gpio_pin_set(gpio, LCD_POWER_PIN, 0);
        gpio_pin_configure(gpio, LCD_EXTCOMIN_PIN, GPIO_DISCONNECTED);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zen_lcd_power, zen_lcd_power_event_handler);
ZMK_SUBSCRIPTION(zen_lcd_power, zmk_activity_state_changed);

static void hide_dashboard(void) {
    lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(connection_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(layer_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(auto_mouse_label, LV_OBJ_FLAG_HIDDEN);
    dashboard_visible = false;
}

static void hide_boot_and_sleep(void) {
    lv_obj_add_flag(logo_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(logo_subtitle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sleep_z_small_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sleep_z_large_label, LV_OBJ_FLAG_HIDDEN);
    sleep_visible = false;
}

static bool is_modifier(uint32_t keycode) {
    return keycode >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL &&
           keycode <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI;
}

static uint8_t modifier_bit(uint32_t keycode) {
    return BIT(keycode - HID_USAGE_KEY_KEYBOARD_LEFTCONTROL);
}

static const char *key_name(uint32_t keycode, char *buffer, size_t size) {
    if (keycode >= HID_USAGE_KEY_KEYBOARD_A && keycode <= HID_USAGE_KEY_KEYBOARD_Z) {
        buffer[0] = 'A' + (keycode - HID_USAGE_KEY_KEYBOARD_A);
        buffer[1] = '\0';
        return buffer;
    }

    if (keycode >= HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION &&
        keycode <= HID_USAGE_KEY_KEYBOARD_9_AND_LEFT_PARENTHESIS) {
        buffer[0] = '1' + (keycode - HID_USAGE_KEY_KEYBOARD_1_AND_EXCLAMATION);
        buffer[1] = '\0';
        return buffer;
    }

    if (keycode == HID_USAGE_KEY_KEYBOARD_0_AND_RIGHT_PARENTHESIS) {
        return "0";
    }

    switch (keycode) {
    case HID_USAGE_KEY_KEYBOARD_RETURN_ENTER:
        return "ENT";
    case HID_USAGE_KEY_KEYBOARD_ESCAPE:
        return "ESC";
    case HID_USAGE_KEY_KEYBOARD_DELETE_BACKSPACE:
        return "BSP";
    case HID_USAGE_KEY_KEYBOARD_TAB:
        return "TAB";
    case HID_USAGE_KEY_KEYBOARD_SPACEBAR:
        return "SPC";
    case HID_USAGE_KEY_KEYBOARD_DELETE_FORWARD:
        return "DEL";
    case HID_USAGE_KEY_KEYBOARD_RIGHTARROW:
        return "RGT";
    case HID_USAGE_KEY_KEYBOARD_LEFTARROW:
        return "LFT";
    case HID_USAGE_KEY_KEYBOARD_DOWNARROW:
        return "DN";
    case HID_USAGE_KEY_KEYBOARD_UPARROW:
        return "UP";
    default:
        snprintf(buffer, size, "K%02X", (unsigned int)keycode);
        return buffer;
    }
}

static void format_key_press(char *text, size_t size, uint32_t keycode, uint8_t modifiers) {
    char key_buffer[12];
    const char *name = key_name(keycode, key_buffer, sizeof(key_buffer));

    if (modifiers != 0) {
        text[0] = '\0';
        if (modifiers & (BIT(0) | BIT(4))) {
            strncat(text, "CTL+", size - strlen(text) - 1);
        }
        if (modifiers & (BIT(1) | BIT(5))) {
            strncat(text, "SFT+", size - strlen(text) - 1);
        }
        if (modifiers & (BIT(2) | BIT(6))) {
            strncat(text, "ALT+", size - strlen(text) - 1);
        }
        if (modifiers & (BIT(3) | BIT(7))) {
            strncat(text, "CMD+", size - strlen(text) - 1);
        }
        strncat(text, name, size - strlen(text) - 1);
        if (strlen(text) > 9) {
            text[0] = '\0';
            if (modifiers & (BIT(0) | BIT(4))) {
                strncat(text, "C", size - strlen(text) - 1);
            }
            if (modifiers & (BIT(1) | BIT(5))) {
                strncat(text, "S", size - strlen(text) - 1);
            }
            if (modifiers & (BIT(2) | BIT(6))) {
                strncat(text, "A", size - strlen(text) - 1);
            }
            if (modifiers & (BIT(3) | BIT(7))) {
                strncat(text, "M", size - strlen(text) - 1);
            }
            strncat(text, "+", size - strlen(text) - 1);
            strncat(text, name, size - strlen(text) - 1);
        }
        return;
    }

    snprintf(text, size, "%s", name);
}

static const char *layer_name(zmk_keymap_layer_index_t layer) {
    switch (layer) {
    case 0:
        return "BASE";
    case 1:
        return "MOUSE";
    case 2:
        return "SYMBOL";
    case 3:
        return "SYSTEM";
    case 4:
        return "SCROLL";
    case 5:
        return "F-KEY";
    case 6:
        return "LAYER6";
    default:
        return "UNKNOWN";
    }
}

static bool split_connected(void) {
    if (active_transport == NULL || active_transport->api == NULL ||
        active_transport->api->get_status == NULL) {
        return false;
    }

    struct zmk_split_transport_status status = active_transport->api->get_status();
    return status.enabled &&
           status.connections == ZMK_SPLIT_TRANSPORT_CONNECTIONS_STATUS_ALL_CONNECTED;
}

static char next_shuffle_char(void) {
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ023456789";

    shuffle_rng_state ^= shuffle_rng_state << 13;
    shuffle_rng_state ^= shuffle_rng_state >> 17;
    shuffle_rng_state ^= shuffle_rng_state << 5;
    return chars[shuffle_rng_state % (sizeof(chars) - 1)];
}

static void build_shuffle_frame(char *output, size_t output_size, const char *target,
                                uint8_t frame, uint8_t total_frames) {
    size_t length = strlen(target);
    size_t limit = MIN(length, output_size - 1);

    for (size_t i = 0; i < limit; i++) {
        char final_char = target[i];
        uint8_t settle_frame = total_frames - ((i * 3 + length) % 4);
        if ((final_char == '~' || final_char == '^') && frame < settle_frame) {
            output[i] = (shuffle_rng_state & 1) ? '~' : '^';
            next_shuffle_char();
        } else if (final_char == ' ' || final_char == '\n' || final_char == '%' ||
                   final_char == '`' || final_char == '-' || frame >= settle_frame) {
            output[i] = final_char;
        } else {
            output[i] = next_shuffle_char();
        }
    }
    output[limit] = '\0';
}

static void set_label_text_shuffled(lv_obj_t *label, char *target, size_t target_size,
                                    uint8_t *frame, const char *text) {
    if (strncmp(target, text, target_size) != 0) {
        snprintf(target, target_size, "%s", text);
        *frame = 1;
    }

    if (*frame == 0) {
        return;
    }

    char shuffled[40];
    build_shuffle_frame(shuffled, sizeof(shuffled), target, *frame, TEXT_SHUFFLE_FRAMES);
    lv_label_set_text(label, shuffled);
    if (*frame >= TEXT_SHUFFLE_FRAMES) {
        *frame = 0;
    } else {
        (*frame)++;
    }
}

#if !defined(CONFIG_ZEN_LCD_DOT_STATUS)
static const char *status_lamp_frame(struct status_lamp *lamp, bool connected) {
    if (!lamp->initialized || lamp->connected != connected) {
        lamp->initialized = true;
        lamp->connected = connected;
        lamp->animation_ticks = connected ? 8 : 0;
    }

    if (!connected) {
        return "--";
    }

    if (lamp->animation_ticks > 4) {
        lamp->animation_ticks--;
        return ". ";
    }

    if (lamp->animation_ticks > 0) {
        lamp->animation_ticks--;
        return "..";
    }

    return "OK";
}
#endif

/* ZMK renamed these after v0.3: zmk_endpoints_selected() became
 * zmk_endpoint_get_selected(), and zmk_endpoint_get_preferred_transport() is
 * new. This tree builds against E24-GH/zmk v0.3-zen, so map them back.
 *
 * v0.3's enum zmk_transport holds only USB and BLE, so a selected endpoint is
 * always one of the two and the fallback below cannot be reached here. On a
 * newer ZMK it can, which is why the branch exists; leave it in place. */
#define zmk_endpoint_get_selected() zmk_endpoints_selected()
#define zmk_endpoint_get_preferred_transport() (zmk_endpoints_selected().transport)

static bool format_host_status(char *text, size_t size) {
    struct zmk_endpoint_instance selected = zmk_endpoint_get_selected();

    if (selected.transport == ZMK_TRANSPORT_USB) {
        snprintf(text, size, "HOST USB");
        return true;
    } else if (selected.transport == ZMK_TRANSPORT_BLE) {
        snprintf(text, size, "HOST BT%d", selected.ble.profile_index + 1);
        return true;
    } else {
        enum zmk_transport preferred = zmk_endpoint_get_preferred_transport();
        if (preferred == ZMK_TRANSPORT_BLE) {
            snprintf(text, size, "HOST BT%d", zmk_ble_active_profile_index() + 1);
        } else {
            snprintf(text, size, "HOST USB");
        }
        return false;
    }
}

static enum zmk_transport current_transport(void) {
    struct zmk_endpoint_instance selected = zmk_endpoint_get_selected();

    if (selected.transport == ZMK_TRANSPORT_USB || selected.transport == ZMK_TRANSPORT_BLE) {
        return selected.transport;
    }

    return zmk_endpoint_get_preferred_transport();
}

static void set_notification(const char *text, int64_t now) {
    snprintf(notification_text, sizeof(notification_text), "%s", text);
    notification_until_ms = now + NOTIFICATION_MS;
    notification_shuffle_frame = 1;
}

static void set_transport_notification(enum zmk_transport transport, uint8_t profile,
                                       int64_t now) {
    char text[sizeof(notification_text)];

    if (transport == ZMK_TRANSPORT_USB) {
        set_notification("HOST USB", now);
    } else {
        snprintf(text, sizeof(text), "BT\nPROFILE %u", profile + 1);
        set_notification(text, now);
    }
}

static void detect_notifications(int64_t now) {
    zmk_keymap_layer_index_t layer = zmk_keymap_highest_layer_active();
    uint8_t ble_profile = zmk_ble_active_profile_index();
    enum zmk_transport transport = current_transport();
    bool right_connected = split_connected();
    char host[24];
    bool host_connected = format_host_status(host, sizeof(host));
    bool left_battery_low = zmk_battery_state_of_charge() <= LOW_BATTERY_PERCENT;
    bool right_battery_low = right_battery_known && right_battery_level <= LOW_BATTERY_PERCENT;
    bool suppress_connection_notifications = now < connection_notifications_after_ms;

    if (!notification_state_initialized) {
        notification_state_initialized = true;
        previous_layer = layer;
        previous_ble_profile = ble_profile;
        previous_transport = transport;
        previous_split_connected = right_connected;
        previous_host_connected = host_connected;
        previous_left_battery_low = left_battery_low;
        previous_right_battery_low = right_battery_low;

        if (left_battery_low) {
            set_notification("LOW\nBATTERY L", now);
        } else if (right_battery_low) {
            set_notification("LOW\nBATTERY R", now);
        }
        return;
    }

    if (previous_split_connected != right_connected) {
        if (!suppress_connection_notifications) {
            set_notification(right_connected ? "RIGHT OK" : "RIGHT\nLOST", now);
        }
        previous_split_connected = right_connected;
    }

    if (previous_host_connected != host_connected) {
        if (!suppress_connection_notifications) {
            if (host_connected) {
                set_transport_notification(transport, ble_profile, now);
            } else {
                set_notification("HOST LOST", now);
            }
        }
        previous_host_connected = host_connected;
    }

    if (previous_transport != transport || previous_ble_profile != ble_profile) {
        if (!suppress_connection_notifications) {
            set_transport_notification(transport, ble_profile, now);
        }
        previous_transport = transport;
        previous_ble_profile = ble_profile;
    }

    if (previous_layer != layer) {
        if (layer >= 2 || (layer == 0 && previous_layer >= 2)) {
            set_notification(layer_name(layer), now);
        }
        previous_layer = layer;
    }

    if (!previous_left_battery_low && left_battery_low) {
        set_notification("LOW\nBATTERY L", now);
    }
    if (!previous_right_battery_low && right_battery_low) {
        set_notification("LOW\nBATTERY R", now);
    }
    previous_left_battery_low = left_battery_low;
    previous_right_battery_low = right_battery_low;
}

static void update_dashboard(void) {
    char text[40];
    char host[24];
    uint8_t left_battery = zmk_battery_state_of_charge();
    zmk_keymap_layer_index_t layer = zmk_keymap_highest_layer_active();
    bool auto_mouse = zmk_keymap_layer_active(zmk_keymap_layer_index_to_id(1));
    bool right_connected = split_connected();
    bool host_connected = format_host_status(host, sizeof(host));
    uint8_t ble_profile = zmk_ble_active_profile_index() + 1;

#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    if (right_battery_known) {
        snprintf(text, sizeof(text), "L%u%% R%u%%", left_battery, right_battery_level);
    } else {
        snprintf(text, sizeof(text), "L%u%% R--%%", left_battery);
    }
#else
    if (right_battery_known) {
        snprintf(text, sizeof(text), "- L%u%% R%u%% BT%u", left_battery,
                 right_battery_level, ble_profile);
    } else {
        snprintf(text, sizeof(text), "- L%u%% R--%% BT%u", left_battery, ble_profile);
    }
#endif
    set_label_text_shuffled(battery_label, last_battery_text, sizeof(last_battery_text),
                            &battery_shuffle_frame, text);

#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    snprintf(text, sizeof(text), "LR`%s HOST`%s", right_connected ? "~" : "^",
             host_connected ? "~" : "^");
#else
    const char *split_frame = status_lamp_frame(&split_lamp, right_connected);
    const char *host_frame = status_lamp_frame(&host_lamp, host_connected);
    snprintf(text, sizeof(text), "- LR %s HOST %s", split_frame, host_frame);
#endif
    set_label_text_shuffled(connection_label, last_connection_text,
                            sizeof(last_connection_text), &connection_shuffle_frame, text);

#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    snprintf(text, sizeof(text), "%s B%u", layer_name(layer), ble_profile);
#else
    snprintf(text, sizeof(text), "- LAYER %s", layer_name(layer));
#endif
    set_label_text_shuffled(layer_label, last_layer_text, sizeof(last_layer_text),
                            &layer_shuffle_frame, text);

#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    snprintf(text, sizeof(text), "AUTO MOUSE %s", auto_mouse ? "ON" : "OFF");
#else
    snprintf(text, sizeof(text), "- AUTO MOUSE %s", auto_mouse ? "ON" : "OFF");
#endif
    set_label_text_shuffled(auto_mouse_label, last_auto_mouse_text,
                            sizeof(last_auto_mouse_text), &auto_mouse_shuffle_frame, text);
}

static void show_dashboard(void) {
    bool replay_shuffle = key_input_visible || notification_visible || screen_blank;

    hide_boot_and_sleep();
    lv_obj_add_flag(key_input_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(connection_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(layer_label, LV_OBJ_FLAG_HIDDEN);
#if !defined(CONFIG_ZEN_LCD_DOT_STATUS)
    lv_obj_clear_flag(auto_mouse_label, LV_OBJ_FLAG_HIDDEN);
#endif

    if (replay_shuffle) {
        battery_shuffle_frame = 1;
        connection_shuffle_frame = 1;
        layer_shuffle_frame = 1;
        auto_mouse_shuffle_frame = 1;
    }

    dashboard_visible = true;
    key_input_visible = false;
    notification_visible = false;
    screen_blank = false;
    update_dashboard();
}

static void show_key_input(const char *text, lv_opa_t opacity) {
    hide_boot_and_sleep();
    hide_dashboard();
    lv_obj_add_flag(notification_label, LV_OBJ_FLAG_HIDDEN);

    if (strcmp(displayed_key_text, text) != 0) {
        snprintf(displayed_key_text, sizeof(displayed_key_text), "%s", text);
        lv_label_set_text(key_input_label, displayed_key_text);
        lv_obj_set_style_text_font(key_input_label,
                                   strlen(displayed_key_text) > 9
                                       ? &zen_font_dot_12
                                       : &zen_font_dot_ui,
                                   LV_PART_MAIN);
    }

    if (last_key_opacity != opacity) {
        last_key_opacity = opacity;
        lv_obj_set_style_text_opa(key_input_label, opacity, LV_PART_MAIN);
    }

    lv_obj_clear_flag(key_input_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(key_input_label, LV_ALIGN_CENTER, 0, 0);
    key_input_visible = true;
    notification_visible = false;
    screen_blank = false;
}

static void show_notification(void) {
    hide_boot_and_sleep();
    lv_obj_add_flag(key_input_label, LV_OBJ_FLAG_HIDDEN);
    hide_dashboard();

    if (strcmp(displayed_notification_text, notification_text) != 0) {
        snprintf(displayed_notification_text, sizeof(displayed_notification_text), "%s",
                 notification_text);
        lv_obj_set_style_text_font(notification_label, &zen_font_dot_ui, LV_PART_MAIN);
        lv_obj_set_style_text_align(notification_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(notification_label, LV_ALIGN_CENTER, 0, 0);
    }
    if (notification_shuffle_frame > 0) {
        char shuffled[sizeof(displayed_notification_text)];
        build_shuffle_frame(shuffled, sizeof(shuffled), displayed_notification_text,
                            notification_shuffle_frame, TEXT_SHUFFLE_FRAMES);
        lv_label_set_text(notification_label, shuffled);
        if (notification_shuffle_frame >= TEXT_SHUFFLE_FRAMES) {
            notification_shuffle_frame = 0;
        } else {
            notification_shuffle_frame++;
        }
    }
    lv_obj_clear_flag(notification_label, LV_OBJ_FLAG_HIDDEN);

    key_input_visible = false;
    notification_visible = true;
    screen_blank = false;
}

static void show_blank_screen(void) {
    hide_boot_and_sleep();
    lv_obj_add_flag(key_input_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label, LV_OBJ_FLAG_HIDDEN);
    hide_dashboard();
    key_input_visible = false;
    notification_visible = false;
    screen_blank = true;
}

static void show_sleep_screen(void) {
    lv_obj_add_flag(key_input_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label, LV_OBJ_FLAG_HIDDEN);
    hide_dashboard();

    lv_label_set_text(logo_label, "ZEN");
    lv_obj_set_style_text_font(logo_label, &zen_font_dot_28, LV_PART_MAIN);
    lv_obj_set_align(logo_label, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(logo_label, 23, 20);
    lv_obj_clear_flag(logo_label, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(sleep_z_small_label, "z");
    lv_obj_set_pos(sleep_z_small_label, 95, 27);
    lv_obj_clear_flag(sleep_z_small_label, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(logo_subtitle, "z");
    lv_obj_set_style_text_font(logo_subtitle, &zen_font_dot_sleep_medium, LV_PART_MAIN);
    lv_obj_set_align(logo_subtitle, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(logo_subtitle, 108, 27);
    lv_obj_clear_flag(logo_subtitle, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(sleep_z_large_label, "z");
    lv_obj_set_pos(sleep_z_large_label, 124, 27);
    lv_obj_clear_flag(sleep_z_large_label, LV_OBJ_FLAG_HIDDEN);

    key_input_visible = false;
    notification_visible = false;
    screen_blank = false;
    sleep_visible = true;
}

static void zen_lcd_timer_cb(lv_timer_t *timer) {
    ARG_UNUSED(timer);

    if (!boot_complete) {
        boot_tick++;
        if (boot_tick <= 10) {
            char shuffled[8];
            build_shuffle_frame(shuffled, sizeof(shuffled), "ZEN", boot_tick, 10);
            lv_label_set_text(logo_label, shuffled);
        } else if (boot_tick == 11) {
            lv_label_set_text(logo_label, "ZEN");
        } else if (boot_tick >= 17 && boot_tick <= 28) {
            char shuffled[12];
            build_shuffle_frame(shuffled, sizeof(shuffled), "HELLO!", boot_tick - 16, 12);
            lv_label_set_text(logo_subtitle, shuffled);
        } else if (boot_tick == 29) {
            lv_label_set_text(logo_subtitle, "HELLO!");
        } else if (boot_tick >= 55) {
            boot_complete = true;
            connection_notifications_after_ms =
                k_uptime_get() + STARTUP_NOTIFICATION_SUPPRESS_MS;
            show_dashboard();
        }
        return;
    }

    int64_t now = k_uptime_get();
    char key_text[sizeof(pending_key_text)];
    int64_t key_press_ms;
    k_spinlock_key_t lock_key = k_spin_lock(&key_input_lock);
    snprintf(key_text, sizeof(key_text), "%s", pending_key_text);
    key_press_ms = last_key_press_ms;
    k_spin_unlock(&key_input_lock, lock_key);

    int64_t idle_ms = now - key_press_ms;

    if (idle_ms >= SLEEP_SCREEN_MS) {
        if (!sleep_visible) {
            show_sleep_screen();
        }
        return;
    }

    detect_notifications(now);

    if (notification_until_ms > now) {
        show_notification();
        return;
    }

    if (key_text[0] != '\0' && idle_ms < KEY_VISIBLE_MS) {
        lv_opa_t opacity = LV_OPA_COVER;
        if (idle_ms > KEY_HOLD_MS) {
            opacity = (lv_opa_t)(((KEY_VISIBLE_MS - idle_ms) * LV_OPA_COVER) / KEY_FADE_MS);
        }
        show_key_input(key_text, opacity);
        return;
    }

    if (key_text[0] != '\0' && idle_ms < STATUS_RETURN_MS) {
        if (!screen_blank) {
            show_blank_screen();
        }
        return;
    }

    if (!dashboard_visible || key_input_visible || notification_visible || screen_blank) {
        show_dashboard();
    }
    update_dashboard();
}

static int zen_lcd_battery_event(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *event =
        as_zmk_peripheral_battery_state_changed(eh);

    if (event != NULL && event->source == 0) {
        right_battery_level = event->state_of_charge;
        right_battery_known = true;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zen_lcd_battery, zen_lcd_battery_event);
ZMK_SUBSCRIPTION(zen_lcd_battery, zmk_peripheral_battery_state_changed);

static int zen_lcd_keycode_event(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *event = as_zmk_keycode_state_changed(eh);

    if (event == NULL || event->usage_page != HID_USAGE_KEY) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (is_modifier(event->keycode)) {
        uint8_t bit = modifier_bit(event->keycode);
        if (event->state) {
            active_modifiers |= bit;
        } else {
            active_modifiers &= ~bit;
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (!event->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    char text[sizeof(pending_key_text)];
    uint8_t modifiers = active_modifiers | event->implicit_modifiers | event->explicit_modifiers;
    format_key_press(text, sizeof(text), event->keycode, modifiers);

    k_spinlock_key_t lock_key = k_spin_lock(&key_input_lock);
    snprintf(pending_key_text, sizeof(pending_key_text), "%s", text);
    last_key_press_ms = k_uptime_get();
    k_spin_unlock(&key_input_lock, lock_key);

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(zen_lcd_keycode, zen_lcd_keycode_event);
ZMK_SUBSCRIPTION(zen_lcd_keycode, zmk_keycode_state_changed);

lv_obj_t *zmk_display_status_screen(void) {
    lv_obj_t *screen = lv_obj_create(NULL);
    status_screen_root = screen;
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    logo_label = lv_label_create(screen);
    lv_obj_set_style_text_font(logo_label, &zen_font_dot_28, LV_PART_MAIN);
    lv_label_set_text(logo_label, "");
    lv_obj_align(logo_label, LV_ALIGN_TOP_MID, 0, 6);

    logo_subtitle = lv_label_create(screen);
    lv_obj_set_style_text_font(logo_subtitle, &zen_font_dot_ui, LV_PART_MAIN);
    lv_label_set_text(logo_subtitle, "");
    lv_obj_align(logo_subtitle, LV_ALIGN_TOP_MID, 0, 43);

    battery_label = lv_label_create(screen);
#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    lv_obj_set_style_text_font(battery_label, &zen_font_dot_status, LV_PART_MAIN);
    lv_obj_set_pos(battery_label, 2, 1);
#else
    lv_obj_set_style_text_font(battery_label, &zen_font_unscii_8x16, LV_PART_MAIN);
    lv_obj_set_pos(battery_label, 0, 1);
#endif
    lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);

    connection_label = lv_label_create(screen);
#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    lv_obj_set_style_text_font(connection_label, &zen_font_dot_status, LV_PART_MAIN);
    lv_obj_set_pos(connection_label, 2, 23);
#else
    lv_obj_set_style_text_font(connection_label, &zen_font_unscii_8x16, LV_PART_MAIN);
    lv_obj_set_pos(connection_label, 0, 18);
#endif
    lv_obj_add_flag(connection_label, LV_OBJ_FLAG_HIDDEN);

    layer_label = lv_label_create(screen);
#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    lv_obj_set_style_text_font(layer_label, &zen_font_dot_status, LV_PART_MAIN);
    lv_obj_set_pos(layer_label, 2, 45);
#else
    lv_obj_set_style_text_font(layer_label, &zen_font_unscii_8x16, LV_PART_MAIN);
    lv_obj_set_pos(layer_label, 0, 35);
#endif
    lv_obj_add_flag(layer_label, LV_OBJ_FLAG_HIDDEN);

    auto_mouse_label = lv_label_create(screen);
#if defined(CONFIG_ZEN_LCD_DOT_STATUS)
    lv_obj_set_style_text_font(auto_mouse_label, &zen_font_dot_status, LV_PART_MAIN);
    lv_obj_set_pos(auto_mouse_label, 4, 52);
#else
    lv_obj_set_style_text_font(auto_mouse_label, &zen_font_unscii_8x16, LV_PART_MAIN);
    lv_obj_set_pos(auto_mouse_label, 0, 52);
#endif
    lv_obj_add_flag(auto_mouse_label, LV_OBJ_FLAG_HIDDEN);

    key_input_label = lv_label_create(screen);
    lv_obj_set_style_text_font(key_input_label, &zen_font_dot_ui, LV_PART_MAIN);
    lv_label_set_text(key_input_label, "");
    lv_obj_align(key_input_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(key_input_label, LV_OBJ_FLAG_HIDDEN);

    notification_label = lv_label_create(screen);
    lv_obj_set_style_text_font(notification_label, &zen_font_dot_ui, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(notification_label, 4, LV_PART_MAIN);
    lv_label_set_text(notification_label, "");
    lv_obj_align(notification_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(notification_label, LV_OBJ_FLAG_HIDDEN);

    sleep_z_small_label = lv_label_create(screen);
    lv_obj_set_style_text_font(sleep_z_small_label, &zen_font_dot_sleep_small, LV_PART_MAIN);
    lv_label_set_text(sleep_z_small_label, "z");
    lv_obj_add_flag(sleep_z_small_label, LV_OBJ_FLAG_HIDDEN);

    sleep_z_large_label = lv_label_create(screen);
    lv_obj_set_style_text_font(sleep_z_large_label, &zen_font_dot_sleep_large, LV_PART_MAIN);
    lv_label_set_text(sleep_z_large_label, "z");
    lv_obj_add_flag(sleep_z_large_label, LV_OBJ_FLAG_HIDDEN);

    last_key_press_ms = k_uptime_get();

    lv_timer_create(zen_lcd_timer_cb, 100, NULL);
    k_timer_start(&panel_static_refresh_timer, K_MSEC(PANEL_STATIC_REFRESH_MS),
                  K_MSEC(PANEL_STATIC_REFRESH_MS));
    return screen;
}
