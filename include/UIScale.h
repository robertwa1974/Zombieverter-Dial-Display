#ifndef UI_SCALE_H
#define UI_SCALE_H

// ============================================
// UI scaling — the layout is authored for the Dial's 240x240 screen.
// S(x) scales a pixel value by SCREEN_WIDTH / 240; UI_FONT_nn (from the board's
// font row in BoardConfig.h) is the font drawn where the Dial uses Montserrat nn.
// On the Dial every macro expands to the original value/font.
//
// Include this LAST in a .cpp: S is a short name and must not reach
// library headers.
// ============================================

#include <lvgl.h>
#include "BoardConfig.h"

#define UI_REF_WIDTH  240   // M5Stack Dial — reference for all layout values

#if SCREEN_WIDTH == UI_REF_WIDTH
#define S(x)  (x)
#elif (SCREEN_WIDTH % UI_REF_WIDTH) == 0
// Whole multiple (e.g. 480): exact, no rounding needed, no runtime divide
#define S(x)  ((x) * (SCREEN_WIDTH / UI_REF_WIDTH))
#else
// x * SCREEN_WIDTH / 240, rounded to nearest (halves away from zero), integer
// math only. x may be evaluated twice — pass plain values/expressions.
#define S(x)  (((x) * SCREEN_WIDTH + ((x) < 0 ? -(UI_REF_WIDTH / 2) : (UI_REF_WIDTH / 2))) \
               / UI_REF_WIDTH)
#endif

#if !defined(UI_FONT_12) || !defined(UI_FONT_14) || !defined(UI_FONT_16) || \
    !defined(UI_FONT_20) || !defined(UI_FONT_28) || !defined(UI_FONT_32) || \
    !defined(UI_FONT_40)
#error "BoardConfig.h: this board is missing its UI font row (UI_FONT_12..UI_FONT_40)"
#endif

// Generated (non-built-in) fonts used by the board's font row
#ifdef UI_FONTS_EXTRA_DECLARE
UI_FONTS_EXTRA_DECLARE
#endif

#endif // UI_SCALE_H
