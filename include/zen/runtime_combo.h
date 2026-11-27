/*
 * Copyright (c) 2026 E24-GH
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <zmk/behavior.h>

/* Editable combo slots, implemented inside ZMK's app/src/combo.c so the
 * matcher and its timing stay upstream's. See E24-GH/zmk v0.3-zen, 30849c7b.
 *
 * Slots are numbered 0..zen_runtime_combo_slot_count()-1 and are separate from
 * the combos the keymap declares, which stay read-only. */

int zen_runtime_combo_slot_count(void);

int zen_runtime_combo_max_keys(void);

/* Writes one slot and rebuilds the combo table. len == 0 clears the slot.
 *
 * Returns 0, -EINVAL for a malformed combo, or -EBUSY when keys are down --
 * rebuilding renumbers combos and an in-flight press holds an index. Nothing
 * is changed on failure. */
int zen_runtime_combo_set(size_t slot, const int32_t *positions, size_t len, uint32_t layer_mask,
                          int32_t timeout_ms, const struct zmk_behavior_binding *binding);

/* Reads a slot back. Any field pointer may be NULL. len is 0 for an empty
 * slot. Returns 0 or -EINVAL for a slot that does not exist. */
int zen_runtime_combo_get(size_t slot, int32_t *positions, size_t *len, uint32_t *layer_mask,
                          int32_t *timeout_ms, struct zmk_behavior_binding *binding);
