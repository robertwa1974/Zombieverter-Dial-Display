#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ============================================
// Board selection
// ============================================
// Each PlatformIO env defines one BOARD_* flag. [env:m5stack-dial] defines
// none, so the M5Stack Dial is the default and that env needs no edits.
#if !defined(BOARD_M5DIAL) && !defined(BOARD_MATOUCH21)
#define BOARD_M5DIAL
#endif

#if defined(BOARD_M5DIAL) && defined(BOARD_MATOUCH21)
#error "Define only one BOARD_* flag"
#endif

// ============================================
// Anatomy of a board entry (see "Adding a new board" in CLAUDE.md)
// ============================================
// Each board section below defines:
//   BOARD_NAME, SCREEN_* — the layout is authored for the Dial's 240 px and
//     S(x) scales it by SCREEN_WIDTH / 240 (UIScale.h)
//   DISPLAY_DRAW_BUF_LINES / DISPLAY_DRAW_BUF_CAPS — LVGL partial buffers
//   pins — ENCODER_*, TOUCH_*, CAN_TX_PIN / CAN_RX_PIN, plus panel pins
//   HAS_RFID / HAS_POWER_HOLD / HAS_LED / HAS_BATTERY
//   a UI font row — UI_FONT_nn is the font drawn wherever the Dial layout uses
//     Montserrat nn. Scale nn by SCREEN_WIDTH / 240 and take the nearest size.
//     Example row for a 360x360 board (x1.5):
//       #define UI_FONT_12  (&lv_font_montserrat_18)
//       #define UI_FONT_14  (&lv_font_montserrat_20)   // 21 -> nearest built-in
//       #define UI_FONT_16  (&lv_font_montserrat_24)
//       #define UI_FONT_20  (&lv_font_montserrat_30)
//       #define UI_FONT_28  (&lv_font_montserrat_42)
//       #define UI_FONT_32  (&lv_font_montserrat_48)
//       #define UI_FONT_40  (&ui_font_montserrat_60)   // generated
//       #define UI_FONTS_EXTRA_DECLARE  LV_FONT_DECLARE(ui_font_montserrat_60)
//     Built-in Montserrat sizes are 8..48 in steps of 2 and must be enabled in
//     lv_conf.h (18/30/42 currently are not). Larger sizes are generated with
//     tools/gen_matouch_fonts.py and declared through UI_FONTS_EXTRA_DECLARE.

#if defined(BOARD_M5DIAL)
// ============================================
// M5Stack Dial — ESP32-S3, 240x240 GC9A01, no PSRAM
// ============================================
#define BOARD_NAME          "M5Stack Dial"

// Display Configuration
#define SCREEN_WIDTH        240
#define SCREEN_HEIGHT       240
#define SCREEN_IS_ROUND     true
#define SCREEN_CENTER_X     120
#define SCREEN_CENTER_Y     120
#define SCREEN_RADIUS       120

// LVGL draw buffers: two partial buffers of SCREEN_WIDTH x N lines
#define DISPLAY_DRAW_BUF_LINES  40
#define DISPLAY_DRAW_BUF_CAPS   (MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)

// Encoder Pins (Built-in on M5Stack Dial)
#define ENCODER_PIN_A       41  // Swapped with PIN_B
#define ENCODER_PIN_B       40  // Swapped with PIN_A
#define ENCODER_BUTTON      42

// Touch Screen (FT6336U)
#define TOUCH_SDA           11
#define TOUCH_SCL           12
#define TOUCH_INT           14
#define TOUCH_RST           -1

// CAN Bus (External Unit - Grove Port)
// USE PORT B (GREY CONNECTOR) - This is the one that works!
// Port B uses GPIO 2 and GPIO 1 (confirmed by testing)
#define CAN_TX_PIN          2   // Port B (Grey) - Pin 1 (Yellow wire)
#define CAN_RX_PIN          1   // Port B (Grey) - Pin 2 (White wire)

// Power & LED
#define POWER_HOLD_PIN      46
#define LED_PIN             21

// Features
#define HAS_RFID            1   // WS1850S on M5.In_I2C, address 0x28
#define HAS_POWER_HOLD      1
#define HAS_LED             1
#define HAS_BATTERY         1

// UI font row — the reference sizes themselves
#define UI_FONT_12          (&lv_font_montserrat_12)
#define UI_FONT_14          (&lv_font_montserrat_14)
#define UI_FONT_16          (&lv_font_montserrat_16)
#define UI_FONT_20          (&lv_font_montserrat_20)
#define UI_FONT_28          (&lv_font_montserrat_28)
#define UI_FONT_32          (&lv_font_montserrat_32)
#define UI_FONT_40          (&lv_font_montserrat_40)

#elif defined(BOARD_MATOUCH21)
// ============================================
// Makerfabs MaTouch ESP32-S3 Rotary 2.1" — ST7701S 480x480 RGB, 8MB OPI PSRAM
// ============================================
#define BOARD_NAME          "MaTouch 2.1"

// Display Configuration
#define SCREEN_WIDTH        480
#define SCREEN_HEIGHT       480
#define SCREEN_IS_ROUND     true
#define SCREEN_CENTER_X     240
#define SCREEN_CENTER_Y     240
#define SCREEN_RADIUS       240

// LVGL draw buffers live in PSRAM; the RGB panel framebuffer is in PSRAM too
#define DISPLAY_DRAW_BUF_LINES  60
#define DISPLAY_DRAW_BUF_CAPS   (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

// ST7701S init interface (3-wire software SPI)
#define LCD_SPI_CS          1
#define LCD_SPI_SCK         46
#define LCD_SPI_MOSI        0

// ST7701S RGB interface
#define LCD_DE              2
#define LCD_VSYNC           42
#define LCD_HSYNC           3
#define LCD_PCLK            45
#define LCD_R0 4
#define LCD_R1 41
#define LCD_R2 5
#define LCD_R3 40
#define LCD_R4 6
#define LCD_G0 39
#define LCD_G1 7
#define LCD_G2 47
#define LCD_G3 8
#define LCD_G4 48
#define LCD_G5 9
#define LCD_B0 11
#define LCD_B1 15
#define LCD_B2 12
#define LCD_B3 16
#define LCD_B4 21
#define LCD_HSYNC_POLARITY  1
#define LCD_HSYNC_FP        10
#define LCD_HSYNC_PW        8
#define LCD_HSYNC_BP        50
#define LCD_VSYNC_POLARITY  1
#define LCD_VSYNC_FP        10
#define LCD_VSYNC_PW        8
#define LCD_VSYNC_BP        20

// Backlight (PWM via ledc)
#define BACKLIGHT_PIN       38
#define BACKLIGHT_LEDC_CH   0
#define BACKLIGHT_LEDC_FREQ 5000

// Encoder (ESP32Encoder full quad, same as Dial) + push button (active low)
#define ENCODER_PIN_A       13  // CLK
#define ENCODER_PIN_B       10  // DT
#define ENCODER_BUTTON      14

// Touch Screen (CST826) — no INT/RST wired. Bus is shareable: a future RFID
// reader can sit on the same I2C lines (see Hardware::i2cBus()).
#define TOUCH_SDA           17
#define TOUCH_SCL           18
#define TOUCH_INT           -1
#define TOUCH_RST           -1
#define TOUCH_I2C_ADDR      0x15

// CST826 -> panel coordinate transform, applied in Hardware::update().
// Unverified defaults: the bring-up Touch screen reports what to set here.
#define TOUCH_SWAP_XY       0
#define TOUCH_MIRROR_X      0
#define TOUCH_MIRROR_Y      0

// CAN Bus — TWAI on the UART connector (external SN65HVD230).
// Console stays on native USB CDC, so GPIO43/44 are free.
#define CAN_TX_PIN          43
#define CAN_RX_PIN          44

// Features
#define HAS_RFID            0   // no reader fitted yet; PIN + BLE unlock only
#define HAS_POWER_HOLD      0
#define HAS_LED             0
#define HAS_BATTERY         0

// UI font row — x2: 24/28/32/40/48/64/80
#define UI_FONT_12          (&lv_font_montserrat_24)
#define UI_FONT_14          (&lv_font_montserrat_28)
#define UI_FONT_16          (&lv_font_montserrat_32)
#define UI_FONT_20          (&lv_font_montserrat_40)
#define UI_FONT_28          (&lv_font_montserrat_48)
// 64/80 px are generated (src/fonts/, tools/gen_matouch_fonts.py) and hold
// only the glyphs their labels use — see the glyph lists in that script
#define UI_FONT_32          (&ui_font_montserrat_64)
#define UI_FONT_40          (&ui_font_montserrat_80)
#define UI_FONTS_EXTRA_DECLARE  LV_FONT_DECLARE(ui_font_montserrat_64) LV_FONT_DECLARE(ui_font_montserrat_80)

#endif

#endif // BOARD_CONFIG_H
