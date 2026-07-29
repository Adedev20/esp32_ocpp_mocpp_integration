# Complete Code Explanation: OCPP EV Charging Station

**Project:** OCPP-based EV Charging Station Controller  
**Platform:** ESP32  
**Purpose:** Manage electric vehicle charging with remote control via Home Assistant using OCPP protocol

---

## Table of Contents

1. [Includes and Architecture Setup](#includes-and-architecture-setup)
2. [Function Prototypes](#function-prototypes)
3. [Configuration Section](#configuration-section)
4. [Hardware Pin Definitions](#hardware-pin-definitions)
5. [Global Variables](#global-variables)
6. [Hardware Control Functions](#hardware-control-functions)
7. [Setup Function](#setup-function)
8. [Loop Function](#loop-function)
9. [Overall Flow](#overall-flow)

---

## Includes and Architecture Setup

**Lines 1-8**

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <MicroOcpp.h>
#include <SPI.h>
#ifdef ARDUINO_ARCH_ESP32
#include <driver/ledc.h>
#endif
```

The code includes essential libraries:

- **`Arduino.h`**: Core Arduino framework for ESP32 development
- **`WiFi.h`**: Network connectivity library for ESP32
- **`MicroOcpp.h`**: Open Charge Point Protocol (OCPP) implementation library
- **`SPI.h`**: Serial Peripheral Interface for hardware communication (potential RFID/sensor use)
- **`driver/ledc.h`**: ESP32-specific LED PWM controller driver, conditionally included only when building for ESP32 architecture

The conditional compilation ensures platform-specific code is only compiled for the target hardware.

---

## Function Prototypes

**Lines 10-13**

```cpp
void onChargingSessionStarted();
void onChargingSessionStopped();
void onLimitChange(float limitAmps);
void inbuilt_led();
```

These are forward declarations of four functions that are used elsewhere in the code but not yet implemented. They should have implementations added to complete the application:

- **`onChargingSessionStarted()`**: Called when an OCPP charging session begins
- **`onChargingSessionStopped()`**: Called when an OCPP charging session ends
- **`onLimitChange(float limitAmps)`**: Called when the charging current limit is updated by the charging management system
- **`inbuilt_led()`**: Controls the status LED blinking pattern

---

## Configuration Section

**Lines 17-24**

```cpp
const char *WIFI_SSID = "NXP";
const char *WIFI_PASS = "Redstones";

const char *OCPP_WS_URL = "ws://192.168.8.237:9000/charger001";
const char *CHARGE_POINT_ID = "charger001";
```

This section defines critical connection parameters:

| Parameter | Value | Purpose |
|-----------|-------|---------|
| `WIFI_SSID` | "NXP" | WiFi network name for ESP32 to connect to |
| `WIFI_PASS` | "Redstones" | WiFi network password |
| `OCPP_WS_URL` | ws://192.168.8.237:9000/charger001 | WebSocket URL of the OCPP Central System (Home Assistant or similar) |
| `CHARGE_POINT_ID` | "charger001" | Unique identifier for this charging station on the network |

---

## Hardware Pin Definitions

**Lines 27-29**

```cpp
#define RELAY_PIN 5       // Contactor / Main relay
#define PWM_CP_PIN 4      // Control Pilot PWM
#define STATUS_LED 15
```

Three GPIO pins on the ESP32 are assigned for hardware control:

| Pin | Function | Purpose |
|-----|----------|---------|
| **GPIO 5** (RELAY_PIN) | Main Relay/Contactor | Switches the high-voltage charging circuit on/off |
| **GPIO 4** (PWM_CP_PIN) | Control Pilot PWM | Generates PWM signal that communicates available charging current to the EV (SAE J1772 standard) |
| **GPIO 15** (STATUS_LED) | Status Indicator LED | Visual feedback of system state |

---

## Global Variables

**Lines 42-43**

```cpp
bool isCharging = false;
float energyWh = 0.0;  // Cumulative energy
```

Two global state variables are maintained:

- **`isCharging`**: Boolean flag tracking whether charging is currently active. Used to prevent redundant state changes.
- **`energyWh`**: Tracks cumulative energy delivered in watt-hours. Currently initialized to 0 and unused; would integrate with an energy meter in production.

---

## Hardware Control Functions

### `turnChargingOn()` — Lines 46-52

```cpp
void turnChargingOn()
{
    Serial.println(F("[EVSE] === Energizing Relay: Charging STARTED ==="));
    digitalWrite(RELAY_PIN, HIGH);
    isCharging = true;
    ledcWrite(PWM_CP_PIN, 820);  // Set initial ~32A pilot signal
}
```

**Purpose**: Enables the charging system

**Operations**:
1. Logs the event via serial for debugging
2. Sets GPIO5 (RELAY_PIN) HIGH to energize the main contactor/relay
3. Updates the global `isCharging` flag to `true`
4. Generates a PWM signal with duty cycle 820/1024 (~80%) representing ~32A maximum charging current

### `turnChargingOff()` — Lines 54-60

```cpp
void turnChargingOff()
{
    Serial.println(F("[EVSE] === De-energizing Relay: Charging STOPPED ==="));
    digitalWrite(RELAY_PIN, LOW);
    isCharging = false;
    ledcWrite(PWM_CP_PIN, 0);  // Shut off the PWM pilot signal
}
```

**Purpose**: Disables the charging system safely

**Operations**:
1. Logs the event via serial
2. Sets GPIO5 (RELAY_PIN) LOW to de-energize the main contactor
3. Updates the global `isCharging` flag to `false`
4. Stops the PWM signal (duty cycle 0) to indicate no charging available

### `updateChargingLimit()` — Lines 62-67

```cpp
void updateChargingLimit(float limitAmps)
{
    Serial.printf("[EVSE] Throttling PWM signal to: %.1f A\n", limitAmps);
    int duty = map(constrain((int)limitAmps, 6, 32), 6, 32, 200, 820);
    ledcWrite(PWM_CP_PIN, duty);
}
```

**Purpose**: Adjust charging current dynamically based on OCPP smart charging commands

**Algorithm**:
1. Logs the requested amperage
2. Constrains the input to the safe range of 6-32A (SAE J1772 standard)
3. Maps the constrained amperage to a 10-bit PWM duty cycle (200-820 range)
4. Updates the PWM output to signal the new current limit to the EV

**Mapping Logic**: 
- 6A → Duty 200 (≈20%)
- 32A → Duty 820 (≈80%)

---

## Setup Function

**Lines 77-140**

```cpp
void setup()
{
    // ... initialization code ...
}
```

The setup function initializes all hardware and establishes connectivity. It runs once at startup.

### Serial Communication — Line 78

```cpp
Serial.begin(115200);
```

Initializes UART communication at 115200 baud for debugging output and monitoring.

### GPIO Configuration — Lines 79-82

```cpp
pinMode(RELAY_PIN, OUTPUT);
pinMode(STATUS_LED, OUTPUT);
digitalWrite(RELAY_PIN, LOW);
```

Configures GPIO pins as outputs and ensures the relay starts in the safe state (OFF).

### PWM Setup — Lines 85-87

```cpp
ledcSetup(0, 1000, 10);
ledcAttachPin(PWM_CP_PIN, 0);
ledcWrite(0, 0);
```

Configures the LEDC (LED PWM Controller) for the Control Pilot signal:
- **Channel 0**: PWM on LEDC channel 0
- **1000 Hz**: Frequency of 1 kHz (standard for SAE J1772)
- **10-bit**: Resolution allows duty cycles from 0-1023
- Initializes with 0% duty cycle for safe startup

### WiFi Connection — Lines 96-101

```cpp
WiFi.begin(WIFI_SSID, WIFI_PASS);
while (WiFi.status() != WL_CONNECTED)
{
    delay(500);
    Serial.print(".");
}
Serial.println("\nWiFi connected!");
Serial.println(WiFi.localIP());
```

Connects to the specified WiFi network and displays the assigned IP address.

### OCPP Initialization — Lines 105

```cpp
mocpp_initialize(OCPP_WS_URL, CHARGE_POINT_ID, "MyEVSE", "ESP32-Charger");
```

Initializes the MicroOcpp library with:
- WebSocket URL of the charging management system
- Unique charge point identifier
- Model and vendor information

### OCPP Callbacks — Lines 113-132

Three critical callbacks are registered to handle OCPP protocol events:

#### Energy Meter Input — Lines 113-117

```cpp
setEnergyMeterInput([]()
{
    return 0.f;  // Return 0 watt-hours for now
});
```

Returns the current energy consumption. Currently returns 0; would integrate with a physical energy meter in production.

#### Smart Charging Current Output — Lines 120-124

```cpp
setSmartChargingCurrentOutput([](float limit)
{
    if (limit >= 0.f) {
        updateChargingLimit(limit);
    }
});
```

Receives charging current limits from the OCPP server and updates the Control Pilot PWM accordingly. The callback only acts on valid (non-negative) limits.

#### Connector Plugged Input — Lines 126-130

```cpp
setConnectorPluggedInput([]()
{
    return true;  // Always return true for testing
});
```

Reports whether an EV is connected. Currently hardcoded to `true` for testing; would check actual connector status in production.

---

## Loop Function

**Lines 142-188**

The loop function is the main control logic that executes repeatedly.

### OCPP Protocol Loop — Line 147

```cpp
mocpp_loop();
```

**Critical**: Must be called frequently to process OCPP messages and maintain the WebSocket connection. Recommended to call every 100-500ms.

### Main Charging State Machine — Lines 154-165

```cpp
if (ocppPermitsCharge())
{
    if (!isCharging)
    {
        turnChargingOn();
    }
}
else
{
    if (isCharging)
    {
        turnChargingOff();
    }
}
```

**Logic Flow**:

| Condition | Current State | Action | Result |
|-----------|---------------|--------|--------|
| OCPP permits charge | Not charging | Call `turnChargingOn()` | Charging starts |
| OCPP permits charge | Already charging | Do nothing | No change |
| OCPP denies charge | Charging | Call `turnChargingOff()` | Charging stops |
| OCPP denies charge | Not charging | Do nothing | No change |

This design prevents redundant function calls and ensures state consistency.

### Disabled RFID Transaction Logic — Lines 173-185

```cpp
if (false)
{
    String idTag = "0123456789ABCD";
    if (!getTransaction())
    {
        auto ret = beginTransaction(idTag.c_str());
        // ...
    }
    else
    {
        if (idTag.equals(getTransactionIdTag()))
        {
            endTransaction(idTag.c_str());
        }
    }
}
```

This block contains transaction management code that would authenticate users via RFID. Currently disabled (`if (false)`) for testing purposes. When enabled, it would:
1. Check if a transaction is active
2. Begin a new transaction if no transaction exists
3. End a transaction when the RFID tag matches

---

## Overall Flow

```
┌─────────────────────────────────────┐
│      ESP32 Boots Up                 │
└────────┬────────────────────────────┘
         │
         ├─► Serial initialization
         ├─► GPIO pin setup
         ├─► PWM configuration
         └─► WiFi connection
             │
             ├─► OCPP library initialization
             └─► Register callbacks
                 │
                 └─► Enter Main Loop
                     │
                     ├─► Process OCPP messages
                     ├─► Check if charging permitted
                     ├─► Maintain relay state
                     ├─► Update PWM based on current limit
                     └─► Repeat every ~100ms
```

### Charging Session Sequence

```
Home Assistant → OCPP Command
                 │
                 ▼
        ocppPermitsCharge() = true
                 │
                 ▼
        isCharging = false?
                 │
                 ├─ YES → turnChargingOn()
                 │        ├─ Relay ON
                 │        ├─ PWM signal sent
                 │        └─ isCharging = true
                 │
                 └─ NO → No action
```

---

## Key Safety Features

1. **Fail-Safe Initialization**: Relay starts in OFF state
2. **Current Limiting**: PWM constrains charging to 6-32A range
3. **State Verification**: `isCharging` flag prevents redundant commands
4. **Serial Logging**: All critical events logged for debugging
5. **OCPP Protocol**: Remote commands only honored through secure OCPP channel

---

## Missing Implementations

The following functions are prototyped but not implemented:

- `onChargingSessionStarted()` — Should handle session startup logic
- `onChargingSessionStopped()` — Should handle session cleanup
- `onLimitChange()` — Should respond to current limit changes
- `inbuilt_led()` — Should provide LED blinking feedback

These should be added to complete the application.

---

## Future Enhancements

- [ ] Integrate RFID reader for user authentication
- [ ] Add energy meter (PZEM-004T) for consumption tracking
- [ ] Implement LED status patterns
- [ ] Add temperature monitoring and thermal cutoff
- [ ] Integrate fault detection and error handling
- [ ] Add persistent logging to SD card

---

**Document Generated**: 2026-07-15  
**Firmware Version**: OCPP EV Charger v1.0 (Baseline)
