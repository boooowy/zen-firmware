// SPDX-License-Identifier: GPL-2.0-or-later
//
// The settings record and its wire representation, with no Zephyr in it, so
// the encode/decode/validate trio can be exercised on the host. It was a bug
// in exactly this trio -- a decoder leaving fields it does not carry at
// whatever was on the stack, and the validator then rejecting them -- that
// silently broke every pointer write, so it is worth being able to test.
//
// The two decoders both take a record they modify in place. That is not an
// optimisation: a pointer payload and an auto-mouse payload each describe only
// part of one shared record, so whatever the caller does not pass in has to
// already be there. See the note on each.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "zen_pointer_settings.h"

#ifdef __cplusplus
extern "C" {
#endif

/** True when every field is in range. Checks the whole record, including the
 * fields a given wire payload knows nothing about. */
bool zen_pointer_settings_is_valid_payload(const struct zen_pointer_settings_payload *payload);

/** Writes ZEN_CONFIG_WIRE_POINTER_PAYLOAD_SIZE bytes. */
void zen_config_encode_pointer(const struct zen_pointer_settings_payload *in, uint8_t *out);

/**
 * Applies a pointer payload to @p record, leaving the auto-mouse fields alone.
 *
 * @param record must already hold a complete record -- typically a copy of
 *               what the keyboard is running. Fields this payload does not
 *               carry are kept as they are.
 * @return 0, or -EINVAL if the result would be out of range (the record is
 *         then left partially updated and must be discarded).
 */
int zen_config_decode_pointer(const uint8_t *in, struct zen_pointer_settings_payload *record);

/** Writes ZEN_CONFIG_WIRE_AUTO_MOUSE_PAYLOAD_SIZE bytes. */
void zen_config_encode_auto_mouse(const struct zen_pointer_settings_payload *in, uint8_t *out);

/** The auto-mouse counterpart of zen_config_decode_pointer; same contract,
 * and it leaves the pointer fields alone. */
int zen_config_decode_auto_mouse(const uint8_t *in, struct zen_pointer_settings_payload *record);

#ifdef __cplusplus
}
#endif
