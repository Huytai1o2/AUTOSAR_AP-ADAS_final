#include "dn_task2_lcd.h"
#include "globals.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdarg.h>

// Instantiate the LCD on I2C address 0x21, 16 columns, 2 rows
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// Helper function to format and pad strings to exactly 16 characters.
// This overwrites previous characters without calling lcd.clear(), which prevents screen flickering.
static void formatLCDLine(char* dest, const char* format, ...) {
    char temp[32];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    int len = strlen(temp);
    if (len > LCD_COLS) {
        len = LCD_COLS;
    }
    memcpy(dest, temp, len);
    for (int i = len; i < LCD_COLS; i++) {
        dest[i] = ' '; // Pad with trailing spaces
    }
    dest[LCD_COLS] = '\0';
}

void vDisplayTask(void *pvParameters) {
    // 1. Initialize LCD (Wire.begin(11, 12) is already called in main.cpp setup)
    lcd.init();
    lcd.backlight();
    lcd.clear();

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(200); // Refresh display every 200ms

    char line1[LCD_COLS + 1];
    char line2[LCD_COLS + 1];

    for (;;) {
        float speed = 0.0f;
        int limit = 120;
        bool limit_active = false;

        // Take the mutex to copy shared system states to local variables
        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            speed = sysState.current_speed;
            limit = sysState.max_limit;
            limit_active = sysState.limit_active;
            xSemaphoreGive(xStateMutex);
        }

        // Format Line 1: "Speed: [speed] km/h"
        // "Speed:%5.1f km/h" generates e.g. "Speed:120.0 km/h" (exactly 16 characters)
        // for speed = 120.0, or "Speed:  0.0 km/h" (exactly 16 characters) for speed = 0.0.
        formatLCDLine(line1, "Speed:%5.1f km/h", speed);

        // Format Line 2: "Limit: [limit] km/h" or "Limit: NONE"
        if (limit_active) {
            formatLCDLine(line2, "Limit: %3d km/h", limit);
        } else {
            formatLCDLine(line2, "Limit: NONE");
        }

        // Write to LCD without clearing to avoid screen flickering
        lcd.setCursor(0, 0);
        lcd.print(line1);

        lcd.setCursor(0, 1);
        lcd.print(line2);

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
