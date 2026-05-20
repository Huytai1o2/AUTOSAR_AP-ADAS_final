#include "dn_task1_motor.h"
#include "globals.h"

void vMotorTask(void *pvParameters) {
    // 1. Initialize digital buttons with internal pull-up resistors (active LOW)
    pinMode(PIN_BTN_UP, INPUT_PULLUP);
    pinMode(PIN_BTN_DOWN, INPUT_PULLUP);

    // 2. Initialize motor control pin as output
    pinMode(PIN_MOTOR_PWM, OUTPUT);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // Runs every 50ms
    const float SPEED_DELTA = 0.5f;                  // Smooth ramping delta (+/-10 km/h per second)

    for (;;) {
        // Read input button states (LOW means pressed due to INPUT_PULLUP)
        bool btn_up = (digitalRead(PIN_BTN_UP) == LOW);
        bool btn_down = (digitalRead(PIN_BTN_DOWN) == LOW);

        if (xSemaphoreTake(xStateMutex, portMAX_DELAY) == pdTRUE) {
            float speed = sysState.current_speed;
            bool limit_active = sysState.limit_active;
            float max_limit = limit_active ? (float)sysState.max_limit : MAX_SPEED_ABS;

            // Deceleration logic (if current speed is greater than max limit)
            if (speed > max_limit) {
                speed -= SPEED_DELTA;
                if (speed < max_limit) {
                    speed = max_limit;
                }
            } 
            // Standard user control ramping logic
            else {
                if (btn_up && !btn_down) {
                    // Accelerate strictly up to max_limit
                    if (speed < max_limit) {
                        speed += SPEED_DELTA;
                        if (speed > max_limit) {
                            speed = max_limit;
                        }
                    }
                } else if (btn_down && !btn_up) {
                    // Decelerate strictly down to 0 km/h
                    speed -= SPEED_DELTA;
                    if (speed < 0.0f) {
                        speed = 0.0f;
                    }
                }
            }

            // Save state
            sysState.current_speed = speed;
            xSemaphoreGive(xStateMutex);

            // Hardware mapping: Scale floating-point speed to an 8-bit integer PWM duty cycle
            int pwm_val = (int)((speed / MAX_SPEED_ABS) * PWM_MAX);
            if (pwm_val < 0) pwm_val = 0;
            if (pwm_val > PWM_MAX) pwm_val = PWM_MAX;

            // Write 8-bit PWM value using analogWrite (supported in ESP32 Arduino framework)
            analogWrite(PIN_MOTOR_PWM, pwm_val);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}