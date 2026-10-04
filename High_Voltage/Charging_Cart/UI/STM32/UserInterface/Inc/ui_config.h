/**
 * @file ui_config.h
 * @brief Configuration for the display, rotary encoder and user interface
 *
 * Sections:
 *   1. Display hardware
 *   2. Rotary encoder
 *   3. User-editable values
 *   4. Fault display
 *
 * The screen layout, fonts and background bitmap are generated from the UI template and
 * live in interfaceTemplate.h.
 */

#ifndef UI_CONFIG_H
#define UI_CONFIG_H

// ============================================================================
// 1. DISPLAY HARDWARE
// ============================================================================

#define FULL_REFRESH 0b11111111
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PAGE_SIZE 8

// Backlight PWM duty (compare value)
#define UI_BACKLIGHT_BRIGHTNESS 255

// Pulse width and settle time for the display reset line (ms)
#define DISPLAY_RESET_DELAY_MS 10

// DISPLAY COMMANDS https://www.hpinfotech.ro/ST7565.pdf
#define CMD_DISPLAY_OFF 0b10101110
#define CMD_DISPLAY_ON 0b10101111
#define CMD_DISPLAY_ALL_ON 0b10100101

// initialization commands
#define CMD_LCD_BIAS 0b10100010
#define CMD_ADC_SELECT 0b10100000   // 0b10100000 for normal, 0b10100001 for reverse
#define CMD_COM_OUTPUT 0b11001000   // 0b11000000 for normal, 0b11001000 for reverse
#define CMD_REG_DIVIDER 0b00100101  // last three bits are the ratio
#define CMD_VOLUME_MODE 0b10000001
#define CMD_SET_VOLUME 0b00011011  // last five bits are volume level
#define CMD_POWER_CNFG 0b00101111  // all stages enabled (last three bits)

#define CMD_SET_PAGE_0 0b10110000   // last four bits are page number
#define CMD_SET_COL_U_0 0b00010000  // last four bits are upper col address
#define CMD_SET_COL_L_0 0b00000000  // last four bits are lower

// ============================================================================
// 2. ROTARY ENCODER
// ============================================================================

// Valid transitions needed to register one detent
#define DETENT_THRESHOLD 4
// Presses closer together than this are ignored (ms)
#define ENCODER_BUTTON_DEBOUNCE_MS 1000

// ============================================================================
// 3. USER-EDITABLE VALUES
// ============================================================================

// Values at start-up
#define UI_DEFAULT_CURRENT_LIMIT 75   // 7.5A
#define UI_DEFAULT_VOLTAGE_LIMIT 250  // 250V

// Ranges the user can select
#define CURRENT_LIMIT_MIN 0
#define CURRENT_LIMIT_MAX 140  // 14A
#define VOLTAGE_LIMIT_MIN 150
#define VOLTAGE_LIMIT_MAX 252

// Leave edit mode after this long without input (ms)
#define UI_EDIT_TIMEOUT_MS 5000

// Value shown for time remaining until a real estimate exists
#define TIME_REMAINING_PLACEHOLDER 1234

// ============================================================================
// 4. FAULT DISPLAY
// ============================================================================

// Time each fault message stays on screen (ms)
#define FAULT_DISPLAY_TIME 1500

// Fault message text, in error_flags bit order (see chargersm_config.h).
// Status-line text is limited to the glyphs in the font.
#define FAULT_MSG_LIST                                                                      \
    /* ABCDEFGHIJK */                                                                       \
    "E HARDWARE", "E TEMP", "E INPUT", "E BATTERY", "E COMMS", "E CAN T\\O", "TBP NOT RDY", \
        "BSM FAULT", "BSM CAN T\\O", "SDC", "CELL O\\V"

#define FAULT_MSG_UNKNOWN "UNKNOWN ERROR"
#define FAULT_MSG_PREFIX "ERR"
// Character offset of the fault message on the status line
#define FAULT_MSG_OFFSET 4

// "[ \ ]" counter showing which fault is displayed out of how many
#define FAULT_COUNT_BRACKETS "[ \\ ]"
#define FAULT_COUNT_BRACKETS_OFFSET 16
#define FAULT_COUNT_INDEX_OFFSET 17
#define FAULT_COUNT_TOTAL_OFFSET 19

#endif  // UI_CONFIG_H
