#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// 1. Hardware Pin Definitions (ESP32-S3)
#define PIN_BTN_UP 6
#define PIN_BTN_DOWN 7
#define PIN_MOTOR_PWM 1

// 2. I2C LCD Configuration
#define LCD_ADDR 0x21
#define LCD_COLS 16
#define LCD_ROWS 2
#define I2C_SDA 11
#define I2C_SCL 12

// 3. System Constants
const float MAX_SPEED_ABS = 120.0f;
const int PWM_MAX = 255;

// 4. Shared Data Structure
struct SystemState {
    float current_speed;  // Ramped speed value (km/h)
    int max_limit;        // Maximum active limit (km/h), defaults to 120
    bool limit_active;    // True if UART speed limit is active
};

// 5. Global state and Mutex Declarations
extern SystemState sysState;
extern SemaphoreHandle_t xStateMutex;

#endif // GLOBALS_H
