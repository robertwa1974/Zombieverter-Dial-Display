#ifndef UI_SCALE_H
#define UI_SCALE_H

// ============================================
// UI scaling — the layout is authored for the Dial's 240x240 screen.
// S(x) scales a pixel value; UI_FONT_nn picks the font for a Dial font size nn.
// On the Dial every macro expands to the original value/font.
//
// Include this LAST in a .cpp: S is a short name and must not reach
// library headers.
// ============================================

#include <lvgl.h>
#include "BoardConfig.h"

#if UI_SCALE == 1

#define S(x)        (x)
#define UI_FONT_12  (&lv_font_montserrat_12)
#define UI_FONT_14  (&lv_font_montserrat_14)
#define UI_FONT_16  (&lv_font_montserrat_16)
#define UI_FONT_20  (&lv_font_montserrat_20)
#define UI_FONT_28  (&lv_font_montserrat_28)
#define UI_FONT_32  (&lv_font_montserrat_32)
#define UI_FONT_40  (&lv_font_montserrat_40)

#elif UI_SCALE == 2

#define S(x)        ((x) * 2)
#define UI_FONT_12  (&lv_font_montserrat_24)
#define UI_FONT_14  (&lv_font_montserrat_28)
#define UI_FONT_16  (&lv_font_montserrat_32)
#define UI_FONT_20  (&lv_font_montserrat_40)
#define UI_FONT_28  (&lv_font_montserrat_48)
// Placeholders until custom 64/80 px fonts are generated
#define UI_FONT_32  (&lv_font_montserrat_48)
#define UI_FONT_40  (&lv_font_montserrat_48)

#else
#error "Unsupported UI_SCALE"
#endif

#endif // UI_SCALE_H
