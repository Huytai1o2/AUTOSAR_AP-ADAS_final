# Motor Speed Limit Control - Implementation Plan

## 1. Configuration & Constants
Define the core configuration variables at the top of the relevant module to allow for easy modification.

*   `MOTOR_UART_PORT`: "/dev/ttyACM1"
*   `MOTOR_INIT_DELAY_SEC`: 0.5
*   `COMMAND_COOLDOWN_SEC`: 2.0 (The minimum delay between sending commands)
*   `MOTOR_ABSOLUTE_MAX`: 120 (Internal max speed when limit is removed)
*   `DEFAULT_RESTRICTED_SPEED`: [Define your desired speed limit 1-100]

## 2. Initialization Phase
Handle the hardware connection and startup delays gracefully.

1.  **Open Port:** Attempt to open `/dev/ttyACM1` (ensure baud rate matches the motor controller hardware specs, which is 115200).
2.  **Initialization Delay:** Apply a blocking sleep/delay for `MOTOR_INIT_DELAY_SEC` (0.5s) immediately after opening the port to allow the motor controller to boot.
3.  **State Tracking:** Initialize a timestamp variable `last_command_timestamp = 0` to track the cooldown period.
4.  **State Tracking:** Initialize `current_limit_state` to track what we last sent, preventing redundant serial writes.

## 3. Communication Protocol Subroutines
Create isolated functions to handle the specific input/output rules of the motor controller.

### `send_speed_limit(limit_value)`
*   **Validation:** Ensure `limit_value` is between `1` and `100`, or exactly `-1` (int type).
*   **Format:** Construct string `"{limit_value}\n"`.
*   **Transmit:** Send over UART.

### `read_motor_response()`
*   **Receive:** Read the incoming line until `\n`.
*   **Parse Success:** If the string starts with `[LIMIT]`, parse the expected format `"[LIMIT] {Applied_Limit} 0\n"`. 
    *   *Note: If `-1` was sent, verify the controller sets this to the `MOTOR_ABSOLUTE_MAX` (120).*
*   **Parse Error:** If the string starts with `[LIMIT-ERR]`, extract the error code `%d`. 
    *   Trigger error handling/logging for the specific motor error code.

## 4. Trigger Logic (The "Decision" Engine)
Within the main control loop, evaluate the environmental sensors to determine the required motor state.

*   **Condition A:** `GPS_Distance <= RANGE_SIZE`
*   **Condition B:** `AI_TrafficLight_Color == RED`
*   **Evaluation:** 
    *   IF (Condition A) OR (Condition B) -> `Target_State = RESTRICTED` (Send 1-100)
    *   ELSE -> `Target_State = UNRESTRICTED` (Send -1)

## 5. Rate-Limited Execution (The "Action" Engine)
Before sending the target state over UART, enforce the configurable delay constraint.

1.  **Check Redundancy:** If `Target_State == current_limit_state`, do nothing (skip to next loop).
2.  **Check Cooldown:** Calculate `Time_Since_Last_Command = Current_Time - last_command_timestamp`.
3.  **Execute:**
    *   IF `Time_Since_Last_Command >= COMMAND_COOLDOWN_SEC` (2.0s):
        *   Call `send_speed_limit(Target_State)`.
        *   Call `read_motor_response()`.
        *   Update `last_command_timestamp = Current_Time`.
        *   Update `current_limit_state = Target_State`.
    *   ELSE:
        *   Command is ignored/queued until the cooldown expires.

Try to build using `./scripts/build_raspi.sh`