#include "Arduino.h"
#include <MicroOcpp.h>

// Global variables for virtual energy meter simulation
float virtualEnergy = 0.0; // Wh

// Energy grows over time
float readVirtualEnergy()
{
    static unsigned long lastMillis = millis();
    unsigned long now = millis();

    // Simulate 2 kW charging rate
    float power = 2000.0; // watts
    float elapsedHours = (now - lastMillis) / 3600000.0;

    virtualEnergy += power * elapsedHours; // Wh

    lastMillis = now;
    return virtualEnergy;
}

void readVirtualMeter()
{
    // Register virtual meter callbacks
    setEnergyMeterInput([]()
                        { return readVirtualEnergy(); });
}