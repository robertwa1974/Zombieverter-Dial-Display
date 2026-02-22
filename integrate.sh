#!/bin/bash
# ============================================================================
# ZombieVerter Display - Automated Integration Helper
# This script helps track and apply manual changes between versions
# ============================================================================

VERSION="1.3.1"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "========================================="
echo "ZombieVerter Display - Integration Helper"
echo "Version: $VERSION"
echo "========================================="
echo ""

# ============================================================================
# Functions
# ============================================================================

check_file_exists() {
    local file=$1
    if [ -f "$file" ]; then
        echo "✅ Found: $file"
        return 0
    else
        echo "❌ Missing: $file"
        return 1
    fi
}

check_git() {
    if git rev-parse --git-dir > /dev/null 2>&1; then
        echo "✅ Git repository detected"
        return 0
    else
        echo "⚠️  Not a git repository"
        return 1
    fi
}

create_backup() {
    local file=$1
    local backup="${file}.backup.$(date +%Y%m%d_%H%M%S)"
    
    if [ -f "$file" ]; then
        cp "$file" "$backup"
        echo "✅ Backed up: $file -> $backup"
        return 0
    else
        echo "❌ Cannot backup: $file not found"
        return 1
    fi
}

show_diff() {
    local original=$1
    local template=$2
    
    if [ -f "$original" ] && [ -f "$template" ]; then
        echo ""
        echo "========================================="
        echo "DIFFERENCES: $original vs $template"
        echo "========================================="
        diff -u "$original" "$template" | head -50
        echo ""
        echo "(Showing first 50 lines of diff)"
        echo ""
    else
        echo "❌ Cannot compare: files missing"
    fi
}

# ============================================================================
# Main Integration Steps
# ============================================================================

echo "Step 1: Checking project structure..."
echo ""

check_file_exists "src/main.cpp"
MAIN_EXISTS=$?

check_file_exists "platformio.ini"
PLATFORMIO_EXISTS=$?

check_file_exists "include/UIManager.h"
UIMANAGER_H_EXISTS=$?

check_file_exists "src/UIManager.cpp"
UIMANAGER_CPP_EXISTS=$?

echo ""

if [ $MAIN_EXISTS -ne 0 ] || [ $PLATFORMIO_EXISTS -ne 0 ]; then
    echo "❌ ERROR: Core files missing. Are you in the project directory?"
    exit 1
fi

echo "========================================="
echo "Step 2: Git Status Check"
echo "========================================="
echo ""

if check_git; then
    echo ""
    echo "Git status:"
    git status -s
    echo ""
    read -p "Create a backup branch? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        BRANCH_NAME="backup-$(date +%Y%m%d_%H%M%S)"
        git checkout -b "$BRANCH_NAME"
        echo "✅ Created backup branch: $BRANCH_NAME"
    fi
else
    echo ""
    read -p "Initialize git repository? (y/n) " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        git init
        git add .
        git commit -m "Initial commit before v$VERSION integration"
        echo "✅ Git repository initialized"
    fi
fi

echo ""
echo "========================================="
echo "Step 3: Version Check"
echo "========================================="
echo ""

echo "Checking current version markers in code..."
echo ""

# Check for v1.3.1 markers
if grep -q "esp_timer_handle_t canHeartbeatTimer" src/main.cpp 2>/dev/null; then
    echo "✅ v1.3.1 detected: Hardware timer present"
elif grep -q "if (uiManager.isEditMode())" src/main.cpp 2>/dev/null; then
    echo "📍 v1.2.0 detected: Edit mode present, hardware timer missing"
    echo "   ⚠️  CRITICAL: Need to add hardware timer (v1.3.1)"
elif grep -q "createLockScreen" src/UIManager.cpp 2>/dev/null; then
    echo "📍 v1.3.0 detected: Lock screen present, need hardware timer"
    echo "   ⚠️  CRITICAL: Need to add hardware timer (v1.3.1)"
else
    echo "📍 v1.1.0 or earlier detected"
    echo "   ⚠️  Multiple updates needed"
fi

echo ""
echo "========================================="
echo "Step 4: Required Changes for v$VERSION"
echo "========================================="
echo ""

echo "Critical changes required:"
echo ""
echo "1. src/main.cpp"
echo "   - Add #include <esp_timer.h>"
echo "   - Add sendCanHeartbeat() function"
echo "   - Add timer setup in setup()"
echo "   - CRITICAL: Prevents BMS timeout"
echo ""
echo "2. platformio.ini"
echo "   - Add miguelbalboa/MFRC522@^1.4.11"
echo "   - Required for built-in RFID"
echo ""

read -p "Show detailed differences? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [ -f "main_COMPLETE_TEMPLATE.cpp" ]; then
        show_diff "src/main.cpp" "main_COMPLETE_TEMPLATE.cpp"
    else
        echo "⚠️  Template file not found: main_COMPLETE_TEMPLATE.cpp"
    fi
fi

echo ""
echo "========================================="
echo "Step 5: Automated Backup"
echo "========================================="
echo ""

read -p "Create backups of files to be modified? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    create_backup "src/main.cpp"
    create_backup "platformio.ini"
    create_backup "include/UIManager.h"
    create_backup "src/UIManager.cpp"
    echo ""
    echo "✅ Backups created with timestamp"
fi

echo ""
echo "========================================="
echo "Step 6: Integration Options"
echo "========================================="
echo ""

echo "Choose an option:"
echo ""
echo "1) Apply changes manually (recommended)"
echo "   - Open CHANGELOG.md"
echo "   - Follow step-by-step instructions"
echo "   - Mark checkboxes as you go"
echo ""
echo "2) Copy template files (quick but overwrites)"
echo "   - Replaces src/main.cpp with template"
echo "   - Requires re-adding custom settings"
echo ""
echo "3) Show file-by-file diff (for advanced users)"
echo "   - View exact changes needed"
echo "   - Apply with your editor"
echo ""
echo "4) Exit and do it later"
echo ""

read -p "Select option (1-4): " -n 1 -r
echo ""

case $REPLY in
    1)
        echo ""
        echo "Opening CHANGELOG.md..."
        if command -v code &> /dev/null; then
            code CHANGELOG.md
        elif command -v nano &> /dev/null; then
            nano CHANGELOG.md
        else
            cat CHANGELOG.md
        fi
        ;;
    2)
        echo ""
        echo "⚠️  WARNING: This will overwrite your src/main.cpp!"
        read -p "Are you sure? (yes/no) " -r
        if [[ $REPLY == "yes" ]]; then
            if [ -f "main_COMPLETE_TEMPLATE.cpp" ]; then
                cp "main_COMPLETE_TEMPLATE.cpp" "src/main.cpp"
                echo "✅ Replaced src/main.cpp with template"
                echo ""
                echo "⚠️  IMPORTANT: You need to re-add:"
                echo "   - Your custom PIN (if changed from 1234)"
                echo "   - Your RFID card UIDs"
                echo "   - Any custom modifications"
            else
                echo "❌ Template file not found!"
            fi
        else
            echo "❌ Cancelled"
        fi
        ;;
    3)
        echo ""
        show_diff "src/main.cpp" "main_COMPLETE_TEMPLATE.cpp"
        ;;
    4)
        echo "Exiting. Run this script again when ready."
        exit 0
        ;;
    *)
        echo "Invalid option"
        exit 1
        ;;
esac

echo ""
echo "========================================="
echo "Step 7: Next Steps"
echo "========================================="
echo ""

echo "✅ Integration helper complete!"
echo ""
echo "Next steps:"
echo "1. Review CHANGELOG.md for complete change list"
echo "2. Apply remaining manual changes"
echo "3. Update platformio.ini (add MFRC522 library)"
echo "4. Build: pio run"
echo "5. Test: Upload and verify CAN heartbeat"
echo "6. Update CHANGELOG.md checkboxes"
echo ""
echo "Critical test: Navigate menus for 60+ seconds"
echo "Expected: No BMS timeout, vehicle keeps running"
echo ""

read -p "Open testing checklist? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "========================================="
    echo "TESTING CHECKLIST - v$VERSION"
    echo "========================================="
    echo ""
    echo "CAN Heartbeat (CRITICAL):"
    echo "[ ] CAN 0x351 sends every 100ms (use CAN analyzer)"
    echo "[ ] Navigate menus for 60+ seconds - no timeout"
    echo "[ ] Enter PIN slowly (20+ seconds) - no timeout"
    echo "[ ] Sit on settings screen - vehicle keeps running"
    echo ""
    echo "RFID (if enabled):"
    echo "[ ] Place card on BACK of M5Dial"
    echo "[ ] Serial shows card UID"
    echo "[ ] Card toggles lock/unlock"
    echo ""
    echo "Edit Mode:"
    echo "[ ] Button on Gear screen enters edit (orange title)"
    echo "[ ] Encoder changes gear in edit mode"
    echo "[ ] Button exits edit mode (white title)"
    echo ""
    echo "General:"
    echo "[ ] All screens display correctly"
    echo "[ ] Encoder navigation works"
    echo "[ ] No compilation errors"
    echo "[ ] No runtime crashes"
    echo ""
fi

echo ""
echo "Done! Good luck with the integration! 🚀"
echo ""
