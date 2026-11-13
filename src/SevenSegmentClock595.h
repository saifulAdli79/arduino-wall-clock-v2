/*
 * SevenSegmentClock595.h
 * 
 * Library for 6-Digit 7-Segment Digital Clock with Calendar, Alarm, and Day Indicators
 * Driven by 3x 74HC595 (24-bit daisy chain) + UDN2981 (+12V source) + ULN2803A (GND sink)
 * Compatible with Arduino Nano (ATmega328P) and DS1307 RTC.
 * 
 * Author: Open Source Community
 * License: MIT
 */

#ifndef SEVEN_SEGMENT_CLOCK_595_H
#define SEVEN_SEGMENT_CLOCK_595_H

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// Operating Modes
enum ClockMode {
    MODE_NORMAL = 0,
    MODE_SET_TIME,
    MODE_SET_DATE,
    MODE_SET_ALARM,
    MODE_STATUS_MSG
};

class SevenSegmentClock595 {
public:
    SevenSegmentClock595();

    /**
     * Initialize pins and hardware
     */
    void begin(uint8_t dataPin = 2, uint8_t clockPin = 3, uint8_t latchPin = 4,
               uint8_t buzzerPin = 10, uint8_t dimmerLedPin = 11);

    /**
     * Configure button pins (Active LOW with internal pull-up)
     */
    void setButtons(uint8_t btnMode = 5, uint8_t btnSet = 6, uint8_t btnUp = 7,
                    uint8_t btnDown = 8, uint8_t btnDimmer = 9);

    /**
     * Call this in loop() - handles multiplexing, timekeeping, buttons, and alarm
     */
    void update();

    // Time & Date setters
    void setTime(int hour, int minute, int second = 0);
    void setDate(int day, int month, int year, int dayOfWeek = 0);
    void setAlarm(int hour, int minute, bool enabled);
    void silenceAlarm();

    // Display helpers
    void showText(const char* text6);
    void setColon(bool active);
    void setAlarmIndicator(bool state);
    void setDayIndicator(uint8_t dayIndex); // 0=Sunday, 1=Monday, ..., 6=Saturday

    // Getters
    int getHour() const { return currentHour; }
    int getMinute() const { return currentMinute; }
    int getSecond() const { return currentSecond; }
    int getDay() const { return currentDay; }
    int getMonth() const { return currentMonth; }
    int getYear() const { return currentYear; }
    int getDayOfWeek() const { return currentDayOfWeek; }
    bool isAlarmEnabled() const { return alarmEnabled; }
    bool isAlarmRinging() const { return alarmRinging; }
    ClockMode getMode() const { return currentMode; }

    // Configuration
    void setMuxInterval(unsigned long intervalUs) { muxIntervalUs = intervalUs; }
    void setDateDisplayDuration(int seconds) { dateDisplayDuration = seconds; }

    // Low-level shift register driver
    void updateShiftRegisters(uint8_t segPattern, uint8_t digitPattern, uint8_t indicatorPattern);
    void shiftByteFast(uint8_t val);

private:
    // Pin configuration
    uint8_t pinData;
    uint8_t pinClock;
    uint8_t pinLatch;
    uint8_t pinBuzzer;
    uint8_t pinDimmerLed;

    struct Button {
        int pin;
        bool lastState;
        unsigned long lastDebounceTime;
        unsigned long pressStartTime;
    };
    Button buttons[5];

    // RTC instance
    RTC_DS1307 rtc;
    bool rtcFound;
    unsigned long lastRtcSyncTime;

    // Display buffers & multiplexing
    uint8_t displayRaw[6];
    int currentMuxDigit;
    bool colonActive;
    uint8_t currentIndicatorByte;
    unsigned long lastMuxTime;
    unsigned long muxIntervalUs;

    // Timekeeping & calendar
    unsigned long lastSecondTick;
    int currentHour;
    int currentMinute;
    int currentSecond;
    int currentDay;
    int currentMonth;
    int currentYear;
    int currentDayOfWeek;

    // Periodic date display settings
    int dateDisplayDuration;
    unsigned long manualDateTimeout;
    bool showingDate;

    // Mode & editing
    ClockMode currentMode;
    int editHour, editMinute;
    int editDay, editMonth, editYear;
    int editField;
    unsigned long lastBlinkTime;
    bool blinkState;
    unsigned long msgTimeout;

    // Alarm
    int alarmHour;
    int alarmMinute;
    bool alarmEnabled;
    bool alarmRinging;
    unsigned long lastBuzzerToggle;
    bool buzzerState;

    bool autoDimmerEnabled;

    // Internal methods
    void multiplexDisplay();
    void updateDisplayData();
    void handleButtons();
    bool checkButtonPressed(int index);
    void checkAlarmTrigger();
    void handleAlarmBuzzer();
    void showAlarmStatusMsg(bool isOn);
    void setTwoDigits(int startIndex, int value, bool shouldBlink);
    void saveSettings();
    void loadSettings();
    static bool isLeapYear(int y);
    static int getDaysInMonth(int m, int y);
    static uint8_t getCharPattern(char c);
};

#endif // SEVEN_SEGMENT_CLOCK_595_H

