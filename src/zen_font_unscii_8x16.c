/*******************************************************************************
 * ZEN status font derived from LVGL's UNSCII font.
 * Glyph size: 8 x 16 px, Bpp: 1, range: U+0020-U+007F.
 * SPDX-License-Identifier: MIT
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl.h"
#endif

#ifndef ZEN_FONT_UNSCII_8X16
#define ZEN_FONT_UNSCII_8X16 1
#endif

#if ZEN_FONT_UNSCII_8X16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xf0, 0xf0,

    /* U+0022 "\"" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x30,

    /* U+0023 "#" */
    0x6c, 0xd9, 0xb3, 0x6f, 0xff, 0xdb, 0x36, 0xff,
    0xfd, 0xb3, 0x66, 0xcd, 0x80,

    /* U+0024 "$" */
    0x30, 0xc7, 0xdf, 0xc3, 0x7, 0x9e, 0xc, 0x3f,
    0xbe, 0x30, 0xc0,

    /* U+0025 "%" */
    0xc7, 0x8f, 0x36, 0x61, 0x83, 0xc, 0x18, 0x66,
    0xcf, 0x1e, 0x30,

    /* U+0026 "&" */
    0x38, 0x71, 0xb3, 0x63, 0x87, 0x1d, 0xbb, 0xdd,
    0xbb, 0x36, 0x67, 0x6e, 0xc0,

    /* U+0027 "'" */
    0x6d, 0xbd, 0x80,

    /* U+0028 "(" */
    0x33, 0x66, 0xcc, 0xcc, 0xcc, 0x66, 0x33,

    /* U+0029 ")" */
    0xcc, 0x66, 0x33, 0x33, 0x33, 0x66, 0xcc,

    /* U+002A "*" */
    0x66, 0x66, 0x3c, 0x3c, 0xff, 0xff, 0x3c, 0x3c,
    0x66, 0x66,

    /* U+002B "+" */
    0x30, 0xc3, 0xc, 0xff, 0xf3, 0xc, 0x30, 0xc0,

    /* U+002C "," */
    0x6d, 0xbd, 0x80,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xff,

    /* U+002F "/" */
    0x3, 0x3, 0x6, 0x6, 0xc, 0xc, 0x18, 0x18,
    0x30, 0x30, 0x60, 0x60, 0xc0, 0xc0,

    /* U+0030 "0" */
    0x79, 0xec, 0xf3, 0xdf, 0x7e, 0xfb, 0xcf, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0031 "1" */
    0x30, 0xc7, 0x1c, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0xc, 0xff, 0xf0,

    /* U+0032 "2" */
    0x79, 0xec, 0xf3, 0x18, 0x63, 0xc, 0x61, 0x8c,
    0x30, 0xff, 0xf0,

    /* U+0033 "3" */
    0x79, 0xec, 0xf3, 0xc, 0x33, 0x8e, 0xc, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0034 "4" */
    0x1c, 0x38, 0xf1, 0xe6, 0xcd, 0xb3, 0x66, 0xff,
    0xfc, 0x30, 0x60, 0xc1, 0x80,

    /* U+0035 "5" */
    0xff, 0xfc, 0x30, 0xfb, 0xe0, 0xc3, 0xc, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0036 "6" */
    0x38, 0xe6, 0x18, 0xc3, 0xf, 0xbe, 0xcf, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0037 "7" */
    0xff, 0xf0, 0xc3, 0xc, 0x31, 0x86, 0x30, 0xc3,
    0xc, 0x30, 0xc0,

    /* U+0038 "8" */
    0x79, 0xec, 0xf3, 0xcf, 0x37, 0x9e, 0xcf, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0039 "9" */
    0x79, 0xec, 0xf3, 0xcf, 0x37, 0xdf, 0xc, 0x31,
    0x86, 0x71, 0xc0,

    /* U+003A ":" */
    0xff, 0x0, 0xff,

    /* U+003B ";" */
    0x6d, 0xb0, 0x0, 0x6d, 0xbd, 0x80,

    /* U+003C "<" */
    0x18, 0xcc, 0x66, 0x33, 0x18, 0x63, 0xc, 0x61,
    0x8c,

    /* U+003D "=" */
    0xff, 0xf0, 0x0, 0xff, 0xf0,

    /* U+003E ">" */
    0xc6, 0x18, 0xc3, 0x18, 0x63, 0x31, 0x98, 0xcc,
    0x60,

    /* U+003F "?" */
    0x79, 0xec, 0xf3, 0xc, 0x31, 0x86, 0x30, 0xc0,
    0x0, 0x30, 0xc0,

    /* U+0040 "@" */
    0x7c, 0xfb, 0x1e, 0x3d, 0xfb, 0xf7, 0xef, 0xdf,
    0xbf, 0x6, 0x7, 0xcf, 0x80,

    /* U+0041 "A" */
    0x30, 0xc7, 0x9e, 0xcf, 0x3c, 0xf3, 0xff, 0xfc,
    0xf3, 0xcf, 0x30,

    /* U+0042 "B" */
    0xfb, 0xec, 0xf3, 0xcf, 0x3f, 0xbe, 0xcf, 0x3c,
    0xf3, 0xfb, 0xe0,

    /* U+0043 "C" */
    0x79, 0xec, 0xf3, 0xc3, 0xc, 0x30, 0xc3, 0xc,
    0xf3, 0x79, 0xe0,

    /* U+0044 "D" */
    0xf3, 0xcd, 0xb6, 0xcf, 0x3c, 0xf3, 0xcf, 0x3d,
    0xb6, 0xf3, 0xc0,

    /* U+0045 "E" */
    0xff, 0xfc, 0x30, 0xc3, 0xf, 0xbe, 0xc3, 0xc,
    0x30, 0xff, 0xf0,

    /* U+0046 "F" */
    0xff, 0xfc, 0x30, 0xc3, 0xf, 0xbe, 0xc3, 0xc,
    0x30, 0xc3, 0x0,

    /* U+0047 "G" */
    0x79, 0xec, 0xf3, 0xc3, 0xd, 0xf7, 0xcf, 0x3c,
    0xf3, 0x7d, 0xf0,

    /* U+0048 "H" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3f, 0xff, 0xcf, 0x3c,
    0xf3, 0xcf, 0x30,

    /* U+0049 "I" */
    0xff, 0xf3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0xc, 0xff, 0xf0,

    /* U+004A "J" */
    0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+004B "K" */
    0xc7, 0x8f, 0x36, 0x6d, 0x9b, 0x3c, 0x78, 0xd9,
    0xb3, 0x36, 0x6c, 0x78, 0xc0,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xc,
    0x30, 0xff, 0xf0,

    /* U+004D "M" */
    0xc7, 0x8f, 0xbf, 0x7f, 0xff, 0xf5, 0xeb, 0xc7,
    0x8f, 0x1e, 0x3c, 0x78, 0xc0,

    /* U+004E "N" */
    0xc7, 0x8f, 0x9f, 0x3f, 0x7e, 0xf7, 0xef, 0xcf,
    0x9f, 0x1e, 0x3c, 0x78, 0xc0,

    /* U+004F "O" */
    0x79, 0xec, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0050 "P" */
    0xfb, 0xec, 0xf3, 0xcf, 0x3f, 0xbe, 0xc3, 0xc,
    0x30, 0xc3, 0x0,

    /* U+0051 "Q" */
    0x79, 0xec, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x3d,
    0xb6, 0x6d, 0xb0,

    /* U+0052 "R" */
    0xfb, 0xec, 0xf3, 0xcf, 0x3f, 0xbe, 0xdb, 0x6c,
    0xf3, 0xcf, 0x30,

    /* U+0053 "S" */
    0x79, 0xec, 0xf3, 0xc3, 0x7, 0x9e, 0xc, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0054 "T" */
    0xff, 0xf3, 0xc, 0x30, 0xc3, 0xc, 0x30, 0xc3,
    0xc, 0x30, 0xc0,

    /* U+0055 "U" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x3c,
    0xf3, 0x79, 0xe0,

    /* U+0056 "V" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x37,
    0x9e, 0x30, 0xc0,

    /* U+0057 "W" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf5, 0xeb, 0xff,
    0xff, 0xbf, 0x7c, 0x78, 0xc0,

    /* U+0058 "X" */
    0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18,
    0x3c, 0x3c, 0x66, 0x66, 0xc3, 0xc3,

    /* U+0059 "Y" */
    0xc3, 0xc3, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0xf0, 0xc3, 0x18, 0x63, 0xc, 0x61, 0x8c,
    0x30, 0xff, 0xf0,

    /* U+005B "[" */
    0xff, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xff,

    /* U+005C "\\" */
    0xc0, 0xc0, 0x60, 0x60, 0x30, 0x30, 0x18, 0x18,
    0xc, 0xc, 0x6, 0x6, 0x3, 0x3,

    /* U+005D "]" */
    0xff, 0x33, 0x33, 0x33, 0x33, 0x33, 0xff,

    /* U+005E "^" */
    0x10, 0x20, 0xe1, 0xc6, 0xcd, 0xb1, 0xe3,

    /* U+005F "_" */
    0xff, 0xff,

    /* U+0060 "`" */
    0xcc, 0x66, 0x33,

    /* U+0061 "a" */
    0x79, 0xe0, 0xc3, 0x7d, 0xfc, 0xf3, 0x7d, 0xf0,

    /* U+0062 "b" */
    0xc3, 0xc, 0x30, 0xfb, 0xec, 0xf3, 0xcf, 0x3c,
    0xf3, 0xfb, 0xe0,

    /* U+0063 "c" */
    0x7b, 0xf1, 0x8c, 0x63, 0x18, 0x7b, 0xc0,

    /* U+0064 "d" */
    0xc, 0x30, 0xc3, 0x7d, 0xfc, 0xf3, 0xcf, 0x3c,
    0xf3, 0x7d, 0xf0,

    /* U+0065 "e" */
    0x79, 0xec, 0xf3, 0xff, 0xfc, 0x30, 0x79, 0xe0,

    /* U+0066 "f" */
    0x39, 0xd8, 0xcf, 0xfd, 0x8c, 0x63, 0x18, 0xc6,
    0x30,

    /* U+0067 "g" */
    0x7d, 0xfc, 0xf3, 0xcf, 0x37, 0xdf, 0xc, 0x3f,
    0xbe,

    /* U+0068 "h" */
    0xc3, 0xc, 0x30, 0xfb, 0xec, 0xf3, 0xcf, 0x3c,
    0xf3, 0xcf, 0x30,

    /* U+0069 "i" */
    0x63, 0x0, 0xe, 0x71, 0x8c, 0x63, 0x18, 0xc7,
    0xbc,

    /* U+006A "j" */
    0x18, 0xc0, 0x1, 0x8c, 0x63, 0x18, 0xc6, 0x31,
    0x8f, 0xde,

    /* U+006B "k" */
    0xc3, 0xc, 0x30, 0xcf, 0x3d, 0xb6, 0xf3, 0xcd,
    0xb6, 0xcf, 0x30,

    /* U+006C "l" */
    0xe7, 0x18, 0xc6, 0x31, 0x8c, 0x63, 0x18, 0xc7,
    0xbc,

    /* U+006D "m" */
    0xcd, 0x9b, 0xff, 0xfd, 0x7a, 0xf5, 0xeb, 0xc7,
    0x8c,

    /* U+006E "n" */
    0xfb, 0xec, 0xf3, 0xcf, 0x3c, 0xf3, 0xcf, 0x30,

    /* U+006F "o" */
    0x79, 0xec, 0xf3, 0xcf, 0x3c, 0xf3, 0x79, 0xe0,

    /* U+0070 "p" */
    0xfb, 0xec, 0xf3, 0xcf, 0x3f, 0xbe, 0xc3, 0xc,
    0x30,

    /* U+0071 "q" */
    0x7d, 0xfc, 0xf3, 0xcf, 0x37, 0xdf, 0xc, 0x30,
    0xc3,

    /* U+0072 "r" */
    0xfb, 0xec, 0xf3, 0xc3, 0xc, 0x30, 0xc3, 0x0,

    /* U+0073 "s" */
    0x7d, 0xfc, 0x30, 0x79, 0xe0, 0xc3, 0xfb, 0xe0,

    /* U+0074 "t" */
    0x61, 0x86, 0x18, 0xff, 0xf6, 0x18, 0x61, 0x86,
    0x18, 0x3c, 0xf0,

    /* U+0075 "u" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xf3, 0x7d, 0xf0,

    /* U+0076 "v" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x37, 0x9e, 0x30, 0xc0,

    /* U+0077 "w" */
    0xc7, 0x8f, 0x1e, 0x3d, 0x7a, 0xdf, 0x3e, 0x6c,
    0xd8,

    /* U+0078 "x" */
    0xc7, 0x8d, 0xb3, 0x63, 0x87, 0x1b, 0x36, 0xc7,
    0x8c,

    /* U+0079 "y" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x37, 0xdf, 0xc, 0x37,
    0x9e,

    /* U+007A "z" */
    0xff, 0xf1, 0x86, 0x30, 0xc6, 0x18, 0xff, 0xf0,

    /* U+007B "{" */
    0x1c, 0x73, 0xc, 0x30, 0xce, 0x38, 0x30, 0xc3,
    0xc, 0x1c, 0x70,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0xe3, 0x83, 0xc, 0x30, 0xc1, 0xc7, 0x30, 0xc3,
    0xc, 0xe3, 0x80,

    /* U+007E "~" */
    0x76, 0xef, 0x76, 0xe0,

    /* U+007F "" */
    0xc1, 0x82, 0x85, 0xa, 0xf5, 0xe9, 0x52, 0xc5,
    0x88, 0x10, 0x20, 0x40, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 128, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 128, .box_w = 2, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 5, .adv_w = 128, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 10},
    {.bitmap_index = 10, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 23, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 34, .adv_w = 128, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 45, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 58, .adv_w = 128, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 10},
    {.bitmap_index = 61, .adv_w = 128, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 68, .adv_w = 128, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 75, .adv_w = 128, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 85, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 4},
    {.bitmap_index = 93, .adv_w = 128, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 128, .box_w = 6, .box_h = 2, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 98, .adv_w = 128, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 99, .adv_w = 128, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 113, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 124, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 135, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 146, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 157, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 170, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 181, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 192, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 203, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 214, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 225, .adv_w = 128, .box_w = 2, .box_h = 12, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 228, .adv_w = 128, .box_w = 3, .box_h = 14, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 234, .adv_w = 128, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 243, .adv_w = 128, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 248, .adv_w = 128, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 257, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 268, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 281, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 292, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 303, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 314, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 325, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 336, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 347, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 358, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 369, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 380, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 391, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 404, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 415, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 428, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 441, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 452, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 463, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 474, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 485, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 496, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 507, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 518, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 529, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 542, .adv_w = 128, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 556, .adv_w = 128, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 570, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 581, .adv_w = 128, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 588, .adv_w = 128, .box_w = 8, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 602, .adv_w = 128, .box_w = 4, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 609, .adv_w = 128, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 8},
    {.bitmap_index = 616, .adv_w = 128, .box_w = 8, .box_h = 2, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 618, .adv_w = 128, .box_w = 4, .box_h = 6, .ofs_x = 0, .ofs_y = 10},
    {.bitmap_index = 621, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 629, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 640, .adv_w = 128, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 647, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 658, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 666, .adv_w = 128, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 675, .adv_w = 128, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 684, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 695, .adv_w = 128, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 704, .adv_w = 128, .box_w = 5, .box_h = 16, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 714, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 725, .adv_w = 128, .box_w = 5, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 734, .adv_w = 128, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 743, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 751, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 759, .adv_w = 128, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 768, .adv_w = 128, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 777, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 785, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 793, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 804, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 812, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 820, .adv_w = 128, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 829, .adv_w = 128, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 838, .adv_w = 128, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 847, .adv_w = 128, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 855, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 866, .adv_w = 128, .box_w = 2, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 870, .adv_w = 128, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 881, .adv_w = 128, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 12},
    {.bitmap_index = 885, .adv_w = 128, .box_w = 7, .box_h = 14, .ofs_x = 0, .ofs_y = 2}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 96, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t zen_font_unscii_8x16 = {
#else
lv_font_t zen_font_unscii_8x16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if ZEN_FONT_UNSCII_8X16*/
