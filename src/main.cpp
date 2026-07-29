#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h> // Include the WiFiManager library
#include <MicroOcpp.h>
#include <SPI.h>
#include <MFRC522.h>
#include <MicroOcpp.h>
#include <MicroOcpp/Core/Context.h>
#include <configs.h>
#include <TM1637TinyDisplay.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoOTA.h>
#ifdef ARDUINO_ARCH_ESP32
#include <driver/ledc.h>
#endif
#include "ota.h"
#include "vmeter.h"
// #include <ModbusMaster.h>

// function prototypes
// void onChargingSessionStarted();
// void onChargingSessionStopped();
// void onLimitChange(float limitAmps);
void enableCharging();
void disableCharging();
void blinkPattern(int duty);
void blinkLED(int times, int duration);
void myLimitFunction(float limit_amps);
// void safelyAbortChargingDueToNetworkLoss();
// void ocppConnectionWatchdog();
//  float getEnergyMeter();

// ====================== CONFIGURATION ======================
// Hardware Pins Defintions
#define RELAY_PIN 9   // Contactor / Main relay
#define PWM_CP_PIN 11 // Control Pilot PWM
#define STATUS_LED 15
#define CONNECTOR_PIN 18
// #define CONNECTOR_JUMPER 5

// Pin assignments for the TM1637 display
#define CLK 3
#define DIO 5

// Instantiate TM1637TinyDisplay Class
TM1637TinyDisplay display(CLK, DIO);

// Display
//  Non-blocking animation states
enum DisplayState
{
    SHOW_HELLO,
    // SHOW_LO,
    CLEAR_SCREEN,
    COUNTING,
    SHOW_TEXT1,
    SHOW_TEXT2,
    CLEAR_HOLD,
    REPEAT_TEXT,
    // DONE
};

DisplayState currentState = SHOW_HELLO;

unsigned long lastStateChange = 0;
int counterValue = 0;
int lastDuty = -1;

// led heartbeat pattern
const long intervalOn = 50;   // ON for 50ms
const long intervalOff = 950; // OFF for 950ms
unsigned long lastBlinkTime = 0;
unsigned long lastDisplayTime = 0;
bool ledState = LOW;

// RFID (MFRC522)
#define RST_PIN 33                // Configurable, see typical pin layout above
#define SS_PIN 8                  // Configurable, see typical pin layout above
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

// Modbus for PZEM-004T (Energy Meter)
#define PZEM_RX 16
#define PZEM_TX 17
// ModbusMaster node;

unsigned long lastToggle = 0;
bool showAmps = true;
float lastLimit = -1; // -1 means no profile yet

// pwm pilot variables
const int pwmDutyCycle = 0;    // Current PWM duty cycle (0-1023 for 10-bit resolution)
const int pwmFrequency = 1000; // PWM frequency in Hz
const int pwmResolution = 10;  // PWM resolution in bits (10 bits for ESP32)

bool isCharging = false;
float energyWh = 0.0; // Cumulative energy
bool plugged;
float localStartingCurrent = 6.0; // Default to 6A, can be overridden by smart charging

// Global timing variables for the safety watchdog
// unsigned long lastConnectedTime = 0;
// const unsigned long WATCHDOG_TIMEOUT = 30000; // 30 seconds grace period
// bool wasConnected = false;

// --- CUSTOM HARDWARE FUNCTIONS ---
void enableCharging()
{
    Serial.println(F("[EVSE] === Energizing Relay: Relay Activated ==="));
    digitalWrite(RELAY_PIN, HIGH);
    display.showString("STRT");
    isCharging = true;
}

// void safelyAbortChargingDueToNetworkLoss()
// {
//     Serial.println(F("[WATCHDOG] Initiating safety shutdown..."));

//     // 1. Drop the PWM pilot to 0 first to signal the car to stop pulling current
//     ledcWrite(PWM_CP_PIN, 0);
//     delay(150); // Safe 150ms delay for zero-current switching

//     // 2. Safely disengage the physical relay once current drops
//     digitalWrite(RELAY_PIN, LOW);
//     isCharging = false;

//     // 3. Inform the local MicroOCPP cache that the transaction is aborted
//     // When HA reconnects, it will read this from LittleFS and log the event properly.
//     endTransaction("Local");

//     Serial.println(F("[WATCHDOG] Safety shutdown complete. System locked offline."));
// }

void disableCharging()
{
    Serial.println(F("[EVSE] === STEP 1: Dropping PWM Pilot to 0Amps (Requesting Car to Stop) ==="));

    // ledcWrite(PWM_CP_PIN, 0); // Safely drop PWM to 0Amps
    // delay(150);               // Wait 150 ms for the car to respond
    Serial.println(F("[EVSE] === De-energizing Relay: Charging STOPPED ==="));
    digitalWrite(RELAY_PIN, LOW);
    // display.clear();
    display.showString("STOP");
    isCharging = false;
}

///////////////////////////////////////////////////////////////////////////////////////////
// IEC 61851 mapping: amps → duty %
int currentToDuty(float amps)
{
    return (int)(amps / 0.6);
}

// Show amps with one decimal place (e.g. 20.0) and decimal point lit
void showAmpsDisplay(float amps)
{
    int whole = (int)amps;
    int decimal = (int)((amps - whole) * 10);

    int value = whole * 10 + decimal;                     // e.g. 200 → "20.0"
    display.showNumberDec(value, true, 4, 0, 0b01000000); // decimal point lit
}

/////////////////////////////////////////////////////////////////////////////////////////////

void updateChargingLimit(float limitAmps)
{
    Serial.printf("[EVSE] Throttling PWM signal to: %.1f A\n", limitAmps);
    // Constrain safety limit between 6A and 32A, map to appropriate 10-bit PWM values
    // int duty = map(constrain((int)limitAmps, 6, 32), 6, 32, 200, 820);
    // ledcWrite(PWM_CP_PIN, duty);
    // myLimitFunction(limitAmps); // Call the official callback to handle the limit change
    // showAmpsDisplay(limitAmps);
    char buffer[6];
    // Format the float variable to 1 decimal place into the buffer string
    dtostrf(limitAmps, 4, 1, buffer);
    display.showString(buffer);

    // 2. Shift the decimal out of the way (e.g., 56.0 becomes 560)
    // int displayInteger = (int)(limitAmps * 10.0);

    // 3. Print the integer, and light up the dot on the 2nd digit using 0x40
    // Syntax: showNumberDec(number, dots bitmask, leading_zeros, length, position)
    // display.showNumberDec(displayInteger, 0x40, false, 3, 0);
}

// void setSmartChargingCurrent(float limit)
// {
//     // Serial.printf("Charging limit set: %.2f A\n", limit);
//     localStartingCurrent = limit;
//     // chargingPower = limit * 230.0;
// }

// void beginSafeTransaction()
// {

//     setSmartChargingCurrent(localStartingCurrent); // Set default starting current to 6A
//     // Serial.printf("Local safe limit applied: %.2f A\n", localStartingCurrent);
//     Serial.println(F("[EVSE] === Local safe limit applied: 6.0 A"));
// }

// Function to handle status led
void blinkSuccess(int count)
{
    for (int i = 0; i < count; i++)
    {
        digitalWrite(STATUS_LED, HIGH);
        delay(100); // On for 100ms
        digitalWrite(STATUS_LED, LOW);
        delay(100); // Off for 100ms
                    // Total cycle time = 200ms per blink
    }
}

// system led status heartbeat
void blinkHeartbeat()
{
    unsigned long currentMillis = millis();

    // Determine the current required interval based on the LED state
    long currentRequiredInterval = ledState ? intervalOn : intervalOff;

    if (currentMillis - lastBlinkTime >= currentRequiredInterval)
    {
        lastBlinkTime = currentMillis;
        ledState = !ledState; // Toggle state
        digitalWrite(STATUS_LED, ledState);
    }
}

void displayStuff()
{
    unsigned long currentMillis = millis();

    switch (currentState)
    {

    case SHOW_HELLO:
        // display.setSegments(SEG_HELL);
        display.showString("HELLO");
        if (currentMillis - lastStateChange >= 1000)
        { // Show "HELL" for 1 second
            lastStateChange = currentMillis;
            currentState = CLEAR_SCREEN;
        }
        break;

        // case SHOW_LO:
        //     display.setSegments(SEG_LO);
        //     if (currentMillis - lastStateChange >= 1000)
        //     { // Show "LO  " for 1 second
        //         lastStateChange = currentMillis;
        //         currentState = CLEAR_SCREEN;
        //     }
        //     break;

    case CLEAR_SCREEN:
        // Clear Screen
        display.clear();
        if (currentMillis - lastStateChange >= 500)
        { // Keep clear for 0.5 seconds
            lastStateChange = currentMillis;
            counterValue = 0; // Reset counter
            currentState = COUNTING;
        }
        break;

    case COUNTING:
        // Increment the counter every 50 milliseconds
        if (currentMillis - lastStateChange >= 300)
        {
            lastStateChange = currentMillis;
            // display.showNumberDec(counterValue, false);
            display.showNumber(counterValue);

            counterValue++;
            if (counterValue > 100)
            {
                currentState = SHOW_TEXT1; // Move to the next state after counting to 100
            }
        }
        break;

    case SHOW_TEXT1:
        display.showString("CHARGE"); // Show your new string
        if (currentMillis - lastStateChange >= 2000)
        { // Keep text on screen for 2 seconds
            lastStateChange = currentMillis;
            currentState = CLEAR_HOLD;
        }
        break;

    case CLEAR_HOLD:
        display.clear(); // Clear the screen
        if (currentMillis - lastStateChange >= 300)
        { // Hold blank for exactly 1 second
            lastStateChange = currentMillis;
            currentState = SHOW_TEXT2; // Move to the next state
        }
        break;

    case SHOW_TEXT2:
        display.showString("FORGE"); // Show your new string
        if (currentMillis - lastStateChange >= 2000)
        { // Keep text on screen for 2 seconds
            lastStateChange = currentMillis;
            currentState = REPEAT_TEXT; // Move to the next state
        }
        break;

    case REPEAT_TEXT:
        // This loops back to SHOW_TEXT to repeat the cycle forever
        display.clear();
        if (currentMillis - lastStateChange >= 1000)
        { // Hold screen completely blank for 1.0 second
            lastStateChange = currentMillis;
            currentState = SHOW_TEXT1;
            break;
        }
    }
}

void wifi_provisioning()
{
    // Create an instance of WiFiManager
    WiFiManager wm;

    // Wipe saved credentials for testing (Optional - uncomment to test portal every boot)
    // wm.resetSettings();

    // Automatically connect using saved credentials.
    // If connection fails, it starts an access point named "ESP32_Setup"
    bool success = wm.autoConnect("My EV_Charger");

    if (!success)
    {
        Serial.println("Failed to connect or hit timeout");
        // ESP.restart();
    }
    else
    {
        // If you get here, you are connected to your WiFi
        Serial.println("Connected to WiFi successfully!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        blinkSuccess(5); // Blink the status LED 5 times to indicate successful connection
    }
}

void rfid_setup()
{
    // RFID Setup
    SPI.begin();        // Init SPI bus
    mfrc522.PCD_Init(); // Init MFRC522
    Serial.println("RFID Initialized");
}

String formatUID(byte *buffer, byte bufferSize)
{
    String uidString = "";
    for (byte i = 0; i < bufferSize; i++)
    {
        if (buffer[i] < 0x10)
            uidString += "0";
        uidString += String(buffer[i], HEX);
        // no space added here
    }
    uidString.toUpperCase();
    return uidString; // "54160788"
}

// // Helper function to format UID bytes into a string
// String formatUID(byte *buffer, byte bufferSize)
// {
//     String uidString = "";
//     for (byte i = 0; i < bufferSize; i++)
//     {
//         if (buffer[i] < 0x10)
//             uidString += "0"; // leading zero
//         uidString += String(buffer[i], HEX);
//         if (i < bufferSize - 1)
//             uidString += " "; // space separator
//     }
//     uidString.toUpperCase();
//     return uidString; // e.g. "56 17 06 78"
// }

// void rfid_check()
// {
//     /*
//      * Simulated RFID logic from official template (left disabled for now)
//      */
//     if (false)
//     {
//         String idTag = "0123456789ABCD";
//         if (!getTransaction())
//         {
//             auto ret = beginTransaction(idTag.c_str());
//             if (ret)
//             {
//                 Serial.println(F("[main] Transaction initiated."));
//             }
//         }
//         else
//         {
//             if (idTag.equals(getTransactionIdTag()))
//             {
//                 endTransaction(idTag.c_str());
//             }
//         }
//     }
// }

void permitCharge()
{
    /*
     * Energize EV plug if OCPP transaction is up and running
     * (Controlled via the Switch component inside Home Assistant)
     */
    if (ocppPermitsCharge())
    {
        if (!isCharging)
        {
            enableCharging();
        }
    }
    else
    {
        if (isCharging)
        {
            disableCharging();
        }
    }
}

void connectorSetup()
{
    setConnectorPluggedInput([]()
                             {
                                 // For your initial test, return true so Home Assistant thinks a car is always plugged in
                                 // bool plugged;
                                 // return plugged = true; });
                                 //return digitalRead(CONNECTOR_PIN) == LOW; // LOW means the connector is plugged in
                                 return true; });
}

void checkConnectorWithReadRfidStatus()
{

    plugged = true;

    // Check for card
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
    {
        String uidString = formatUID(mfrc522.uid.uidByte, mfrc522.uid.size);
        Serial.print("Card UID: ");
        Serial.println(uidString);

        // If plugged in and no transaction yet → start with UID
        if (plugged && !getTransaction())
        {
            beginTransaction(uidString.c_str()); // use UID as idTag
            // display.showString("START");
            //  digitalWrite(RELAY_PIN, HIGH);
            //   beginSafeTransaction();
        }
    }

    // If unplugged and transaction active → end it
    if (!plugged && getTransaction())
    {
        endTransaction();
        // digitalWrite(RELAY_PIN, LOW);
    }

    // bool plugged = digitalRead(CONNECTOR_PIN) == HIGH;

    // // If plugged in and no transaction yet → start one
    // if (plugged && !getTransaction())
    // {
    //     Serial.println("Plugged in → Starting transaction");
    //     beginTransaction("auto");      // start transaction with idTag "auto"
    //     digitalWrite(RELAY_PIN, HIGH); // close relay
    // }

    // // If unplugged and transaction active → end it
    // else if (!plugged && getTransaction())
    // {
    //     Serial.println("Unplugged → Ending transaction");
    //     endTransaction();             // stop transaction
    //     digitalWrite(RELAY_PIN, LOW); // open relay
    // }
}

void checkEnergyMeter()
{
    /*
     * Integrate OCPP functionality using the official callbacks
     */
    setEnergyMeterInput([]()
                        {
                            return 50.f; // Return 0 watt-hours for now if you don't have a physical meter IC
                        });
}

void checkSmartChargingLimit()
{
    // The OFFICIAL callback used to catch Smart Charging limits from Home Assistant or Steve's OCPP server. This is the correct way to handle it, not the commented-out version below.
    setSmartChargingCurrentOutput([](float limit)
                                  {

        if (limit >= 0.f) {
            updateChargingLimit(limit);
        } });
    // setSmartChargingCurrentOutput(setSmartChargingCurrent);
    // setSmartChargingOutput(myLimitFunction);
}

void pwmPilotInit()
{
    // 1. PWM for Control Pilot (1kHz, 10-bit)
    ledcAttach(PWM_CP_PIN, pwmFrequency, pwmResolution);
    ledcWrite(PWM_CP_PIN, 0); // Safe state initially
}

///////////////////////////////////////////////////////////////////////////////////////////////

void setup()
{
    Serial.begin(115200);
    display.begin();
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(STATUS_LED, OUTPUT);
    pinMode(PWM_CP_PIN, OUTPUT);
    pinMode(CONNECTOR_PIN, INPUT_PULLDOWN); // Set the connector pin as input with pull-up res
    // pinMode(CONNECTOR_JUMPER, OUTPUT);
    // digitalWrite(CONNECTOR_JUMPER, HIGH); //Enable the jumper to simulate a plugged-in state
    digitalWrite(RELAY_PIN, LOW); // Ensure relay is off at startup
    // Say Hello
    display.showString("CHARGE FORGE LTD");
    // delay(2000); // Show the startup message for 2 seconds
    //   Show zeros until Steve sends a profile
    display.showNumber(0000, true);
    // display.clear();
    rfid_setup(); // Initialize RFID reader

    // 1. PWM for Control Pilot (1kHz, 10-bit)
    // pwmPilotInit();

    // 3. Modbus for PZEM
    // Serial2.begin(9600, SERIAL_8N1, PZEM_RX, PZEM_TX);
    // node.begin(1, Serial2);   // Slave ID = 1

    // 4. WiFi
    wifi_provisioning();

    // 5. Initialize MicroOcpp
    mocpp_initialize(OCPP_WS_URL, CHARGE_POINT_ID, "Demo Charger1", "ChargeForge Ltd");
    connectorSetup();   // Set up the connector plugged input callback
    readVirtualMeter(); // Set up the virtual energy meter callback
    // checkEnergyMeter();        // Set up the energy meter callback
    checkSmartChargingLimit(); // Set up the smart charging limit callback
    ota_prov();                // local OTA server for firmware updates

    // Serial.println("[SYSTEM] Setup complete. Awaiting connection to Home Assistant...");
    Serial.println("[SYSTEM] Setup complete. Awaiting connection to Ocpp Server...");
}

void loop()
{
    // put your main code here, to run repeatedly:
    mocpp_loop();                       // Critical - call often
    checkConnectorWithReadRfidStatus(); // Check if the EV is plugged in  & read RFID status
    // Serial.print("connector pin state: ");
    // Serial.println(digitalRead(CONNECTOR_PIN));
    // updateDisplay(); // Update the display with the latest limit or duty cycle
    // displayStuff();   // Update the display every second
    blinkHeartbeat(); // Blink the status LED once to indicate the loop is running

    /*
     * Energize EV plug if OCPP transaction is up and running
     * (Controlled via the Switch component inside Home Assistant)
     */
    permitCharge();
    // Handle OTA updates and web server requests
    ArduinoOTA.handle();
    server.handleClient();
}
