// SPDX-License-Identifier: GPL-2.0-or-later
//
// Rotates a sensor's (x, y) delta by the angle the user picked in the
// Configurator, so a trackball mounted at an angle still moves the pointer
// straight up when the ball is rolled "away".
//
// This lives next to the sensor drivers rather than in a ZMK input processor
// because rotation mixes the two axes, and a processor only ever sees one
// input_event at a time: the driver reports INPUT_REL_X and INPUT_REL_Y
// separately, and by the time the Y event arrives the X one has already been
// handed on. A driver has both values in hand before it reports either.
//
// Call it on the deltas the driver is about to report, before any inversion
// the user has configured -- the rotation aligns the sensor's frame with the
// user's hand, and invert_x/invert_y are a preference applied on top of that.

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-sensor carry for the fractional part of a rotation. Rounding every
 * report on its own would swallow sub-count motion and make slow movement
 * sticky, the same reason the live-scale processor tracks remainders. Keep
 * one of these alongside each sensor's own data. */
struct zen_pointer_rotate_state {
    int32_t remainder_x;
    int32_t remainder_y;
};

/**
 * Rotate (*x, *y) in place by the configured trackball angle.
 *
 * A no-op (and leaves the remainders alone) when the angle is 0, which is
 * the default, so builds where nobody has touched the setting pay only a
 * comparison per report.
 */
void zen_pointer_rotate(int16_t *x, int16_t *y, struct zen_pointer_rotate_state *state);

#ifdef __cplusplus
}
#endif
