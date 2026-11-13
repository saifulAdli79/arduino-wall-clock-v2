/*
 * =========================================================================================
 * PROJECT : 6-Digit 7-Segment Digital Clock with Calendar, Alarm, and Day Indicators
 * TARGET  : Arduino Nano (ATmega328P, 5V Logic)
 * DRIVERS : 3x 74HC595 (24-bit Daisy Chain) + UDN2981 (+12V High-Side) + ULN2803A (GND Sink)
 * RTC     : DS1307 (I2C Real-Time Clock Module)
 * DEMO    : https://youtu.be/lr2jxXmumGY
 * LICENSE : MIT License
 * =========================================================================================
 * 
 * ARDUINO NANO PINOUT:
 * - D2  -> 74HC595 DS (Serial Data)
 * - D3  -> 74HC595 SH_CP (Shift Clock)
 * - D4  -> 74HC595 ST_CP (Storage Latch)
 * - D5  -> MODE Button (INPUT_PULLUP to GND)
 * - D6  -> SET Button (INPUT_PULLUP to GND)
 * - D7  -> UP (+) Button (INPUT_PULLUP to GND)
 * - D8  -> DOWN (-) Button / Date Peek (INPUT_PULLUP to GND)
 * - D9  -> DIMMER Button (INPUT_PULLUP to GND)
 * - D10 -> Active Buzzer
 * - D11 -> Dimmer Mode LED Indicator
 * - D13 -> Nano Onboard LED (1-Second Heartbeat Pulse)
 * - A4  -> DS1307 RTC SDA (I2C Data)
 * - A5  -> DS1307 RTC SCL (I2C Clock)
 * =========================================================================================
 */

#include "src/SevenSegmentClock595.h"

// Hardware Pin Definitions
const uint8_t PIN_DATA       = 2;   // 74HC595 DS (Pin 14 on U1)
const uint8_t PIN_CLOCK      = 3;   // 74HC595 SH_CP (Pin 11 on U1, U2, U3)
const uint8_t PIN_LATCH      = 4;   // 74HC595 ST_CP (Pin 12 on U1, U2, U3)
const uint8_t PIN_BUZZER     = 10;  // Alarm Piezo Buzzer
const uint8_t PIN_DIMMER_LED = 11;  // Dimmer Mode Indicator LED

// Pushbutton Pins (Active LOW, internal pull-up enabled)
const uint8_t BTN_MODE       = 5;   // Mode selection button
const uint8_t BTN_SET        = 6;   // Field toggle / settings button
const uint8_t BTN_UP         = 7;   // Increment / Alarm toggle button
const uint8_t BTN_DOWN       = 8;   // Decrement / Manual date peek button
const uint8_t BTN_DIMMER     = 9;   // Dimmer mode button (optional)

// Create Clock Instance
SevenSegmentClock595 clockDisplay;

void setup() {
    // 1. Initialize clock display and output driver pins
    clockDisplay.begin(PIN_DATA, PIN_CLOCK, PIN_LATCH, PIN_BUZZER, PIN_DIMMER_LED);

    // 2. Configure pushbutton pins
    clockDisplay.setButtons(BTN_MODE, BTN_SET, BTN_UP, BTN_DOWN, BTN_DIMMER);

    // 3. (Optional) Customize display settings
    // clockDisplay.setDateDisplayDuration(5); // Show date for 5 seconds every minute
    // clockDisplay.setMuxInterval(2500);      // 2.5ms per digit (flicker-free ~67 Hz)
}

void loop() {
    // Continuously handles multiplexing, timekeeping, calendar cycle, buttons, and alarm
    clockDisplay.update();
}
