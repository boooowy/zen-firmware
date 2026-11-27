/*
 * Copyright (c) 2026 E24-GH
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/* Editable macro slots, played by the zen,behavior-macro behavior.
 *
 * A slot is a short list of steps. Playback queues every step into ZMK's own
 * behavior queue the moment the macro is triggered, so a slot rewritten while
 * a macro is running cannot derail it -- the queue already holds copies. */

enum zen_macro_op {
    ZEN_MACRO_OP_TAP = 0,     /* press then release `value` */
    ZEN_MACRO_OP_PRESS = 1,   /* press `value` and leave it down */
    ZEN_MACRO_OP_RELEASE = 2, /* release `value` */
    ZEN_MACRO_OP_WAIT = 3,    /* wait `value` milliseconds */
};

struct zen_macro_step {
    uint8_t op;
    uint8_t reserved[3];
    uint32_t value; /* a ZMK keycode, or milliseconds for WAIT */
};

int zen_macro_slot_count(void);

int zen_macro_max_steps(void);

/* Replaces a slot. count == 0 empties it. Returns 0 or -EINVAL.
 *
 * Unlike a combo, this never answers -EBUSY: a running macro reads nothing
 * from the slot, so there is no moment when a write would be unsafe. */
int zen_macro_set(size_t slot, const struct zen_macro_step *steps, size_t count);

/* Reads a slot back. steps may be NULL to ask only for the count. */
int zen_macro_get(size_t slot, struct zen_macro_step *steps, size_t *count);

/* Writes the slots to the settings partition, and empties them again. */
int zen_macro_settings_save(void);
int zen_macro_settings_reset(void);
