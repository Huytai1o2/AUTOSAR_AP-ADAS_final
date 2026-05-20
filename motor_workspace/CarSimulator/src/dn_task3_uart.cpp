#include "dn_task3_uart.h"
#include "globals.h"

void vUartTask(void *pvParameters) {
    String inputBuffer = "";
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // Poll Serial buffer every 100ms

    for (;;) {
        // Read all available bytes from the Serial port buffer
        while (Serial.available() > 0) {
            char c = Serial.read();

            // Handle newline or carriage return as command delimiter
            if (c == '\n' || c == '\r') {
                inputBuffer.trim(); // Clean extra spaces
                if (inputBuffer.length() > 0) {
                    int value = inputBuffer.toInt();

                    if (value >= 1 && value <= 100) {
                        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
                            sysState.max_limit = value;
                            sysState.limit_active = true;
                            xSemaphoreGive(xStateMutex);
                            Serial.printf("[SYSTEM] Speed limit successfully set to: %d km/h\n", value);
                        }
                    } else if (value == -1) {
                        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
                            sysState.limit_active = false;
                            sysState.max_limit = (int)MAX_SPEED_ABS;
                            xSemaphoreGive(xStateMutex);
                            Serial.println("[SYSTEM] Speed limit deactivated. Default 120 km/h restored.");
                        }
                    } else {
                        Serial.println("[WARNING] Invalid UART Command. Supported: 1 to 100, or -1 to clear.");
                    }
                    inputBuffer = ""; // Flush the buffer
                }
            } else {
                // Accumulate digits and negative signs to support "-1"
                if (isDigit(c) || c == '-') {
                    inputBuffer += c;
                }
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
