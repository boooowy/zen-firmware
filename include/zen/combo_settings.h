/*
 * Copyright (c) 2026 E24-GH
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* Persistence for the editable combo slots.
 *
 * The slots themselves live in ZMK's combo.c (see runtime_combo.h); this only
 * copies them to and from the settings partition. Saving is explicit, the way
 * the pointer settings are: an edit takes effect immediately but survives a
 * power cycle only once the host asks for it, so trying something out never
 * commits it. */

/* Writes the slots as they stand now. */
int zen_combo_settings_save(void);

/* Clears every slot and removes the saved record, so the keyboard comes back
 * on the combos compiled into the firmware. */
int zen_combo_settings_reset(void);
