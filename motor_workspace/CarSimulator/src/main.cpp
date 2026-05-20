/*
Define inputs: 

- Switch 1 (D4 - GPIO7): Increase the speed of the fan (holding it, increase for each 1 second)
- Switch 2 (D5 - GPIO8): Decrease the speed of the fan (holding it, decrease for each 1 second)

Define outputs: 

- Motor (A0 - GPIO1): The fan (using Analog Write output, (from 0 to 255), but simulate as the speed of the car (Max is 120 km/h)
- LCD 16x2 Output (I2C: SDA-GPIO11, SCL-GPIO12; Addr: 0x21): Print the Current speed 

Define tasks (running on freeRTOS): 

- Task 1: Controlling Speed
  + The speed can be control by switch 1 and switch 2
  + The speed will be limited to a number when there is an UART signal
    = There is a master will send an UART to this device (n (from 1 - 100) for the max speed, -1 for removing the max speed), a flag called MAX_SPEED_FLAG will set, and a variable for max_speed will set.
    = If the motor_speed > max_speed, then decrease the motor to the max_speed, and the user cannot exceed the max speed via switch 1 until there is an -1 signal is coming from the master.

- Task 2: Display Speed
  + The speed will be displayed on the LCD screen in the format of "Speed: n km/h", where n is the current speed.
  + When the MAX_SPEED_FLAG is true, the display will show "Limited: n km/h", where n is the max speed.

- Task 3: Handling UART Signal
  + The device will read the data from the UART port.
  + If the data is a number between 1 and 100, set the max_speed to that number and set the MAX_SPEED_FLAG to true.
  + If the data is -1, set the MAX_SPEED_FLAG to false.

Other requirements: 

- Each tasks should be on specific file. 
- The holding button should adding smooth effect when the speed is increasing or decreasing.
- The UART use will be on UART1, but for now, please develop on UART0 (for simulation and verification). 
- using vTaskDelay for non-blocking delay.


*/

#include <Arduino.h>
#include <Wire.h>
#include "globals.h"
#include "dn_task1_motor.h"
#include "dn_task2_lcd.h"
#include "dn_task3_uart.h"

// 1. Instantiate the global state
SystemState sysState = {
    .current_speed = 0.0f,
    .max_limit = (int)MAX_SPEED_ABS,
    .limit_active = false
};

// 2. Instantiate the global Mutex
SemaphoreHandle_t xStateMutex = NULL;

void setup() {
    // 3. Initialize Serial Communication at 115200 baud
    Serial.begin(115200);
    delay(1000); // Give the ESP32 hardware UART a moment to initialize
    // Delay for 10 seconds 
    // for (auto i = 0; i < 10; ++i){
    //     Serial.println("[STARTUP] Waiting for the UART to connect...");
    //     delay(1000); 
    // }
    Serial.println("[STARTUP] Fan Speed Controller starting up...");

    // 4. Initialize the custom ESP32-S3 I2C pins for the LCD (SDA=11, SCL=12)
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("[STARTUP] Wire I2C initialized on SDA=11, SCL=12");

    // 5. Create the FreeRTOS Mutex
    xStateMutex = xSemaphoreCreateMutex();
    if (xStateMutex == NULL) {
        Serial.println("[FATAL ERROR] State Mutex creation failed! System halted.");
        while (1);
    }
    Serial.println("[STARTUP] FreeRTOS Mutex created successfully");

    // 6. Create FreeRTOS Tasks (All pinned to Application Core 1)
    
    // Task 1: Motor control - high priority (3)
    xTaskCreatePinnedToCore(
        vMotorTask,
        "MotorTask",
        4096,
        NULL,
        3,
        NULL,
        1
    );
    Serial.println("[STARTUP] Task 1 (Motor Control) started on Core 1");

    // Task 2: LCD display - low priority (1)
    xTaskCreatePinnedToCore(
        vDisplayTask,
        "DisplayTask",
        4096,
        NULL,
        1,
        NULL,
        1
    );
    Serial.println("[STARTUP] Task 2 (LCD Display) started on Core 1");

    // Task 3: UART Listener - medium priority (2)
    xTaskCreatePinnedToCore(
        vUartTask,
        "UartTask",
        4096,
        NULL,
        2,
        NULL,
        1
    );
    Serial.println("[STARTUP] Task 3 (UART Listener) started on Core 1");

    Serial.println("[STARTUP] All initialization tasks executed.");
}

void loop() {
    // Under Arduino on ESP32, loop() runs as a persistent FreeRTOS task.
    // Deleting the loop task reclaims its stack and memory allocation since 
    // our controllers are completely driven by the three specialized tasks.
    vTaskDelete(NULL);
}