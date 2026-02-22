@echo off
REM ============================================================================
REM ZombieVerter Display - Integration Helper (Windows)
REM This script helps track and apply manual changes between versions
REM ============================================================================

set VERSION=1.3.1

echo =========================================
echo ZombieVerter Display - Integration Helper
echo Version: %VERSION%
echo =========================================
echo.

REM ============================================================================
REM Check project structure
REM ============================================================================

echo Step 1: Checking project structure...
echo.

if not exist "src\main.cpp" (
    echo ERROR: src\main.cpp not found
    echo Are you in the project directory?
    pause
    exit /b 1
)

if not exist "platformio.ini" (
    echo ERROR: platformio.ini not found
    echo Are you in the project directory?
    pause
    exit /b 1
)

echo OK: Found src\main.cpp
echo OK: Found platformio.ini
echo.

REM ============================================================================
REM Check version
REM ============================================================================

echo Step 2: Checking current version...
echo.

findstr /C:"esp_timer_handle_t canHeartbeatTimer" src\main.cpp >nul 2>&1
if %errorlevel% equ 0 (
    echo OK: v1.3.1 detected - Hardware timer present
) else (
    findstr /C:"if (uiManager.isEditMode())" src\main.cpp >nul 2>&1
    if %errorlevel% equ 0 (
        echo WARNING: v1.2.0 detected - Need hardware timer update
        echo CRITICAL: This prevents BMS timeout!
    ) else (
        echo WARNING: v1.1.0 or earlier detected
        echo Multiple updates needed
    )
)
echo.

REM ============================================================================
REM Create backups
REM ============================================================================

echo Step 3: Creating backups...
echo.

set TIMESTAMP=%date:~-4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%

if exist "src\main.cpp" (
    copy "src\main.cpp" "src\main.cpp.backup.%TIMESTAMP%" >nul
    echo OK: Backed up src\main.cpp
)

if exist "platformio.ini" (
    copy "platformio.ini" "platformio.ini.backup.%TIMESTAMP%" >nul
    echo OK: Backed up platformio.ini
)

if exist "include\UIManager.h" (
    copy "include\UIManager.h" "include\UIManager.h.backup.%TIMESTAMP%" >nul
    echo OK: Backed up UIManager.h
)

echo.

REM ============================================================================
REM Show required changes
REM ============================================================================

echo Step 4: Required changes for v%VERSION%
echo =========================================
echo.

echo CRITICAL CHANGES:
echo.
echo 1. src\main.cpp
echo    - Add: #include ^<esp_timer.h^>
echo    - Add: sendCanHeartbeat() function
echo    - Add: Timer setup in setup()
echo    PREVENTS: BMS timeout during menu use
echo.
echo 2. platformio.ini
echo    - Add: miguelbalboa/MFRC522@^1.4.11
echo    ENABLES: Built-in RFID reader
echo.

pause

REM ============================================================================
REM Integration options
REM ============================================================================

echo.
echo Step 5: Integration Options
echo =========================================
echo.
echo 1) Manual integration (RECOMMENDED)
echo    - Open CHANGELOG.md in your editor
echo    - Follow step-by-step instructions
echo    - Mark checkboxes as you complete each step
echo.
echo 2) Quick copy template (overwrites src\main.cpp)
echo    - Replaces file with complete template
echo    - Need to re-add custom settings after
echo.
echo 3) View differences
echo    - See what changed between versions
echo.
echo 4) Exit
echo.

set /p CHOICE="Select option (1-4): "

if "%CHOICE%"=="1" (
    echo.
    echo Opening CHANGELOG.md...
    if exist "CHANGELOG.md" (
        start CHANGELOG.md
    ) else (
        echo ERROR: CHANGELOG.md not found!
    )
    goto :testing
)

if "%CHOICE%"=="2" (
    echo.
    echo WARNING: This will OVERWRITE src\main.cpp!
    echo Backup has been created
    echo.
    set /p CONFIRM="Type YES to confirm: "
    if /i "%CONFIRM%"=="YES" (
        if exist "main_COMPLETE_TEMPLATE.cpp" (
            copy /Y "main_COMPLETE_TEMPLATE.cpp" "src\main.cpp" >nul
            echo.
            echo OK: Replaced src\main.cpp with template
            echo.
            echo IMPORTANT: Re-add your custom settings:
            echo - PIN code (default is 1234)
            echo - RFID card UIDs
            echo - Any custom modifications
        ) else (
            echo ERROR: main_COMPLETE_TEMPLATE.cpp not found!
        )
    ) else (
        echo Cancelled
    )
    goto :testing
)

if "%CHOICE%"=="3" (
    echo.
    if exist "main_COMPLETE_TEMPLATE.cpp" (
        echo Comparing files...
        fc /N "src\main.cpp" "main_COMPLETE_TEMPLATE.cpp" | more
    ) else (
        echo ERROR: Template file not found!
    )
    pause
    goto :testing
)

if "%CHOICE%"=="4" (
    echo.
    echo Exiting. Run this script again when ready.
    exit /b 0
)

echo Invalid option
pause
exit /b 1

REM ============================================================================
REM Testing checklist
REM ============================================================================

:testing
echo.
echo =========================================
echo TESTING CHECKLIST - v%VERSION%
echo =========================================
echo.

echo After applying changes, test:
echo.
echo CAN Heartbeat (CRITICAL):
echo [ ] Monitor CAN bus - 0x351 every 100ms
echo [ ] Navigate menus for 60+ seconds - no timeout
echo [ ] Enter PIN slowly - no timeout
echo [ ] Vehicle keeps running during menu use
echo.
echo RFID (if enabled):
echo [ ] Place card on BACK of M5Dial
echo [ ] Serial monitor shows card UID
echo [ ] Card toggles lock/unlock
echo.
echo Edit Mode:
echo [ ] Button enters edit mode (orange title)
echo [ ] Encoder adjusts values
echo [ ] Button exits edit mode (white title)
echo.
echo General:
echo [ ] Compiles without errors
echo [ ] All screens work
echo [ ] No crashes
echo.

pause

echo.
echo =========================================
echo Next Steps
echo =========================================
echo.
echo 1. Review CHANGELOG.md for complete details
echo 2. Apply remaining manual changes
echo 3. Update platformio.ini (add MFRC522)
echo 4. Build: pio run
echo 5. Upload and test
echo 6. Mark CHANGELOG.md checkboxes complete
echo.
echo Critical: Test for BMS timeout!
echo Expected: No timeout while using menus
echo.

pause
