# FreeRTOS Fan Speed Controller - Project Plan (Arduino (PlatformIO) ESP32-S3)

## 1. Hardware & Pin Definitions (ESP32-S3)
*Taking advantage of ESP32-S3's flexible GPIO routing and Arduino core features.*

* **Inputs (Internal Pull-ups Recommended):**
    * `PIN_BTN_UP` (GPIO7): Increase speed.
    * `PIN_BTN_DOWN` (GPIO8): Decrease speed.
* **Outputs:**
    * `PIN_MOTOR_PWM` (GPIO1): PWM output for fan control. (Using ESP32 Arduino `analogWrite()` or `ledc` API).
    * `I2C_LCD`: SDA (GPIO11), SCL (GPIO12), Address `0x21`. (Initialized via `Wire.begin(11, 12)`).
* **Constants:**
    * `MAX_SPEED_ABS = 120` (Absolute maximum km/h).
    * `PWM_MAX = 255` (8-bit resolution).
    * `UART_PORT = Serial` (UART0 for standard USB/Serial monitor simulation; swap to `Serial1` for production).

---

## 2. FreeRTOS Architecture & Shared Resources

* **Shared Data Structure:** A global `SystemState` struct containing:
    * `float current_speed` (Using a float allows for the smooth ramping/holding effect).
    * `int max_limit` (Defaults to 120; updated by UART).
    * `bool limit_active` (Flag for UART override, replaces `MAX_SPEED_FLAG`).
* **Resource Protection:** 
    * Use a **FreeRTOS Mutex** (`SemaphoreHandle_t xStateMutex = xSemaphoreCreateMutex();`) to protect reads/writes to the `SystemState` struct, preventing race conditions between the dual cores of the ESP32-S3.

---

## 3. Task Breakdown & Logic

### Task 1: Motor & Speed Control (`dn_task1_motor.cpp`)
* **Priority:** High.
* **Core Affinity:** Core 1 (Application Core) or unpinned.
* **Frequency:** Runs every ~50ms using `vTaskDelay(pdMS_TO_TICKS(50))`.
* **Logic:** 
    * **Button Smoothing:** Read button states using `digitalRead`. If held LOW (assuming pull-ups), increment/decrement `current_speed` by a small delta (e.g., `+0.5 km/h` per loop). 
    * **Speed Limiter:** Lock the Mutex, check `limit_active` and `max_limit`. If `current_speed > max_limit`, smoothly decelerate down to the limit. Block the `PIN_BTN_UP` from increasing speed if at or above the limit.
    * **Hardware Execution:** Map the float speed to an integer PWM value: `PWM_Value = (current_speed / MAX_SPEED_ABS) * PWM_MAX`. Write to `PIN_MOTOR_PWM`.

### Task 2: LCD Display (`dn_task2_display.cpp`)
* **Priority:** Low.
* **Frequency:** Runs every ~200ms using `vTaskDelay`.
* **Logic:**
    * Lock the Mutex, copy `current_speed`, `limit_active`, and `max_limit` to local variables, then unlock.
    * **Line 1:** Print `"Speed: [current_speed] km/h"`.
    * **Line 2:** If `limit_active` is true, print `"Limit: [max_limit] km/h"`. If false, print `"Limit: NONE"` or clear the line.

### Task 3: UART Command Listener (`dn_task3_uart.cpp`)
* **Priority:** Medium.
* **Frequency:** Blocked/Polled every 100ms.
* **Logic:** 
    * Check `Serial.available()`. Listen for ASCII string inputs terminated by a newline (`\n`). 
    * Convert the received string to an integer (can utilize `Serial.parseInt()`).
    * If `1 <= value <= 100`: Lock Mutex, set `max_limit = value`, set `limit_active = true`.
    * If `value == -1`: Lock Mutex, set `limit_active = false`, reset `max_limit = 120`.
    * Other value: Do nothing. 

---

## 4. File & Folder Structure
*Using the requested `dn_taskx_[name]` convention, optimized for PlatformIO.*

* **`main.cpp`**: Hardware initialization (`Serial.begin`, `Wire.begin`, `pinMode`), Mutex creation, `xTaskCreate` calls for the three tasks.
* **`globals.h`**: Defines the `SystemState` struct, Mutex `extern` declarations, pin mapping macros, and shared constants.
* **`dn_task1_motor.h` / `.cpp`**: Button reading, speed math, PWM `analogWrite` implementation.
* **`dn_task2_lcd.h` / `.cpp`**: I2C `Wire` initialization and LCD printing implementation.
* **`dn_task3_uart.h` / `.cpp`**: `Serial` reading and string parsing implementation.