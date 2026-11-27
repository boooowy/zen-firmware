// SPDX-License-Identifier: GPL-2.0-or-later

#include "zen_pointer_rotate.h"

#include "zen_pointer_settings.h"

/* sin(0deg) .. sin(90deg) in Q15. cos(t) is sin(90 - t), so one table covers
 * both; the settings layer clamps the angle to +/-45deg, which keeps every
 * index below in range. No FPU on an nRF52840, and a 182-byte table beats
 * pulling in libm for one multiply. */
static const int32_t sin_q15[91] = {
         0,    572,   1144,   1715,   2286,   2856,   3425,   3993,
      4560,   5126,   5690,   6252,   6813,   7371,   7927,   8481,
      9032,   9580,  10126,  10668,  11207,  11743,  12275,  12803,
     13328,  13848,  14365,  14876,  15384,  15886,  16384,  16877,
     17364,  17847,  18324,  18795,  19261,  19720,  20174,  20622,
     21063,  21498,  21926,  22348,  22763,  23170,  23571,  23965,
     24351,  24730,  25102,  25466,  25822,  26170,  26510,  26842,
     27166,  27482,  27789,  28088,  28378,  28660,  28932,  29197,
     29452,  29698,  29935,  30163,  30382,  30592,  30792,  30983,
     31164,  31336,  31499,  31651,  31795,  31928,  32052,  32166,
     32270,  32365,  32449,  32524,  32588,  32643,  32688,  32723,
     32748,  32763,  32767,
};

#define Q15_ONE 32768

/* Carry the fractional part forward instead of rounding it away, so a slow
 * roll still moves the pointer. Matches apply_scale() in the live-scale
 * processor: the remainder keeps the sign of the value it came from, and its
 * magnitude stays below Q15_ONE. */
static int16_t take_whole(int32_t scaled, int32_t *remainder) {
    scaled += *remainder;

    const int32_t whole = scaled / Q15_ONE;
    *remainder = scaled - (whole * Q15_ONE);

    return (int16_t)whole;
}

void zen_pointer_rotate(int16_t *x, int16_t *y, struct zen_pointer_rotate_state *state) {
    if (!x || !y || !state) {
        return;
    }

    const int8_t degrees = zen_pointer_settings_payload()->rotation_degrees;
    if (degrees == 0) {
        return;
    }

    /* Positive = clockwise on screen, which is what "ボールの上を右に傾ける"
     * means for someone looking down at the keyboard. */
    const int32_t sin_t = degrees < 0 ? -sin_q15[-degrees] : sin_q15[degrees];
    const int32_t cos_t = sin_q15[90 - (degrees < 0 ? -degrees : degrees)];

    const int32_t in_x = *x;
    const int32_t in_y = *y;

    *x = take_whole(in_x * cos_t - in_y * sin_t, &state->remainder_x);
    *y = take_whole(in_x * sin_t + in_y * cos_t, &state->remainder_y);
}
