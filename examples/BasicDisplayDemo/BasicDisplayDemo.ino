/*
 * BasicDisplayDemo.ino
 * 
 * Demonstrates simple manual control of the 6-Digit 7-Segment Display:
 * - Setting and displaying time manually
 * - Displaying custom alphanumeric text ("HELLO ", "ONLINE")
 * - Controlling day LEDs and colons directly
 * 
 * Circuit Demo: https://youtu.be/lr2jxXmumGY
 * License: MIT
 */

#include <SevenSegmentClock595.h>

const uint8_t PIN_DATA  = 2;
const uint8_t PIN_CLOCK = 3;
const uint8_t PIN_LATCH = 4;

SevenSegmentClock595 myDisplay;

void setup() {
    myDisplay.begin(PIN_DATA, PIN_CLOCK, PIN_LATCH);
    
    // Display greeting text
    myDisplay.showText("HELLO ");
    myDisplay.setDayIndicator(0); // Light up Sunday LED
    myDisplay.setColon(true);
}

void loop() {
    // Continuously refreshes the multiplexed display
    myDisplay.update();
}

