/*
 * ClockWithAlarm.ino
 * 
 * Example sketch demonstrating full clock functionality using SevenSegmentClock595:
 * - 6-Digit Time Display (HH:MM:SS)
 * - Automatic Periodic Calendar Display (DD MM YY) at the end of each minute
 * - 7 Day-of-the-week LEDs & Alarm Status LED
 * - Hardware Button Menu (Mode, Set, Up, Down, Dimmer)
 * - Programmable Alarm saved to DS1307 battery-backed NVRAM
 * 
 * Circuit Demo: https://youtu.be/lr2jxXmumGY
 * License: MIT
 */

#include <SevenSegmentClock595.h>

// 74HC595 Shift Register Pins
const uint8_t PIN_DATA       = 2;   // DS (Pin 14 on U1)
const uint8_t PIN_CLOCK      = 3;   // SH_CP (Pin 11 on U1, U2, U3)
const uint8_t PIN_LATCH      = 4;   // ST_CP (Pin 12 on U1, U2, U3)

// Peripherals
const uint8_t PIN_BUZZER     = 10;  // Alarm Piezo Buzzer
const uint8_t PIN_DIMMER_LED = 11;  // Dimmer Mode Indicator LED

// Pushbutton Pins (Active LOW to GND, internal pull-up enabled)
const uint8_t BTN_MODE       = 5;
const uint8_t BTN_SET        = 6;
const uint8_t BTN_UP         = 7;
const uint8_t BTN_DOWN       = 8;
const uint8_t BTN_DIMMER     = 9;

// Clock instance
SevenSegmentClock595 myClock;

void setup() {
    // 1. Initialize clock with display & buzzer pins
    myClock.begin(PIN_DATA, PIN_CLOCK, PIN_LATCH, PIN_BUZZER, PIN_DIMMER_LED);

    // 2. Attach pushbuttons
    myClock.setButtons(BTN_MODE, BTN_SET, BTN_UP, BTN_DOWN, BTN_DIMMER);

    // 3. (Optional) Customize settings
    // myClock.setDateDisplayDuration(5); // Show date for 5s at end of each minute (seconds 55-59)
    // myClock.setMuxInterval(2500);      // 2.5ms refresh per digit (flicker-free ~67 Hz)
}

void loop() {
    // Continuously runs display multiplexing, timekeeping, buttons, and alarm
    myClock.update();
}

