// ============================================================================
// LOCK SCREEN IMPLEMENTATION
// Add this to UIManager.cpp after createSplashScreen()
// ============================================================================

void UIManager::createLockScreen() {
    screens[SCREEN_LOCK] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screens[SCREEN_LOCK], lv_color_black(), 0);
    
    // Lock icon (large padlock symbol)
    lock_icon = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_icon, LV_SYMBOL_LOCK);  // Built-in padlock symbol
    lv_obj_set_style_text_font(lock_icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lock_icon, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lock_icon, LV_ALIGN_CENTER, 0, -60);
    
    // Title
    lock_title_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_title_label, "VEHICLE LOCKED");
    lv_obj_set_style_text_font(lock_title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_align(lock_title_label, LV_ALIGN_CENTER, 0, -10);
    
    // PIN display (shows entered digits as dots)
    lock_pin_display = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_pin_display, "_ _ _ _");
    lv_obj_set_style_text_font(lock_pin_display, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lock_pin_display, lv_color_white(), 0);
    lv_obj_align(lock_pin_display, LV_ALIGN_CENTER, 0, 30);
    
    // Current digit selector
    lock_digit_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_digit_label, "0");
    lv_obj_set_style_text_font(lock_digit_label, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(lock_digit_label, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_align(lock_digit_label, LV_ALIGN_CENTER, 0, 75);
    
    // Instruction
    lock_instruction_label = lv_label_create(screens[SCREEN_LOCK]);
    lv_label_set_text(lock_instruction_label, "Rotate: Select  |  Click: Enter");
    lv_obj_set_style_text_font(lock_instruction_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lock_instruction_label, lv_palette_darken(LV_PALETTE_GREY, 1), 0);
    lv_obj_set_style_text_align(lock_instruction_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lock_instruction_label, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void UIManager::updateLockScreen() {
    if (!immobilizer) return;
    
    // Update PIN display (show dots for entered digits)
    uint8_t pinPos = immobilizer->getPINPosition();
    char pinDisplay[16];
    
    for (int i = 0; i < 4; i++) {
        if (i < pinPos) {
            pinDisplay[i * 2] = '*';  // Entered digit
        } else {
            pinDisplay[i * 2] = '_';  // Empty slot
        }
        pinDisplay[i * 2 + 1] = ' ';  // Space
    }
    pinDisplay[7] = '\0';  // Null terminator
    
    lv_label_set_text(lock_pin_display, pinDisplay);
    
    // Update current digit
    lv_label_set_text_fmt(lock_digit_label, "%d", immobilizer->getCurrentDigit());
    
    // Change color based on locked state
    if (immobilizer->isUnlocked()) {
        lv_obj_set_style_text_color(lock_icon, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_label_set_text(lock_title_label, "UNLOCKED");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_obj_set_style_text_color(lock_icon, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(lock_title_label, "VEHICLE LOCKED");
        lv_obj_set_style_text_color(lock_title_label, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

// ============================================================================
// UPDATE UIManager::init() to create lock screen
// Add this line after createSplashScreen():
// ============================================================================
/*
    createLockScreen();
*/

// ============================================================================
// UPDATE UIManager constructor to initialize immobilizer pointer
// Change the initialization list to:
// ============================================================================
/*
UIManager::UIManager() 
    : canManager(nullptr), immobilizer(nullptr), currentScreen(SCREEN_SPLASH), 
      lastUpdateTime(0), buf1(nullptr), buf2(nullptr), editMode(false) {
    instance = this;
*/
