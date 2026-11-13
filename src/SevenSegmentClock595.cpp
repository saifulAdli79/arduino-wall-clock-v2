/*
 * SevenSegmentClock595.cpp
 * 
 * Implementation of 6-Digit 7-Segment Digital Clock Library
 * License: MIT
 */

#include "SevenSegmentClock595.h"

// -------------------------------------------------------------------------
// Constant Tables & Segment Maps
// -------------------------------------------------------------------------
// Segment bits: Bit 0=A, Bit 1=B, Bit 2=C, Bit 3=D, Bit 4=E, Bit 5=F, Bit 6=G, Bit 7=COLON
static const uint8_t SEGMENT_MAP[10] = {
    0b00111111, // '0' (A, B, C, D, E, F)
    0b00000110, // '1' (B, C)
    0b01011011, // '2' (A, B, D, E, G)
    0b01001111, // '3' (A, B, C, D, G)
    0b01100110, // '4' (B, C, F, G)
    0b01101101, // '5' (A, C, D, F, G)
    0b01111101, // '6' (A, C, D, E, F, G)
    0b00000111, // '7' (A, B, C)
    0b01111111, // '8' (A, B, C, D, E, F, G)
    0b01101111  // '9' (A, B, C, D, F, G)
};

static const uint8_t BIT_COLON     = 0b10000000; // Bit 7 on U1
static const uint8_t BIT_ALARM_LED = 0b10000000; // Bit 7 on U3
static const uint8_t CHAR_BLANK    = 0b00000000;
static const uint8_t CHAR_A        = 0b01110111;
static const uint8_t CHAR_L        = 0b00111000;
static const uint8_t CHAR_O        = 0b00111111;
static const uint8_t CHAR_N        = 0b01010100;
static const uint8_t CHAR_F        = 0b01110001;

// Digit Cathode Sink mapping on ULN2803A (from U2)
static const uint8_t DIGIT_MAP[6] = {
    (1 << 0),                         // Digit 1 (Q0)
    (1 << 1),                         // Digit 2 (Q1)
    (1 << 2),                         // Digit 3 (Q2)
    (1 << 3),                         // Digit 4 (Q3)
    (1 << 4),                         // Digit 5 (Q4)
    (1 << 5) | (1 << 6) | (1 << 7)     // Digit 6 (Q5 / Q6 / Q7)
};

// DS1307 Non-Volatile RAM (NVRAM) addresses
#define NVRAM_MAGIC_ADDR 0
#define NVRAM_HOUR_ADDR  1
#define NVRAM_MIN_ADDR   2
#define NVRAM_EN_ADDR    3
#define MAGIC_BYTE       0xD5

// -------------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------------
SevenSegmentClock595::SevenSegmentClock595()
    : pinData(2), pinClock(3), pinLatch(4), pinBuzzer(10), pinDimmerLed(11),
      rtcFound(false), lastRtcSyncTime(0),
      currentMuxDigit(0), colonActive(true), currentIndicatorByte(0x01),
      lastMuxTime(0), muxIntervalUs(2500),
      lastSecondTick(0), currentHour(12), currentMinute(0), currentSecond(0),
      currentDay(1), currentMonth(1), currentYear(2026), currentDayOfWeek(0),
      dateDisplayDuration(5), manualDateTimeout(0), showingDate(false),
      currentMode(MODE_NORMAL),
      editHour(12), editMinute(0), editDay(1), editMonth(1), editYear(2026), editField(0),
      lastBlinkTime(0), blinkState(true), msgTimeout(0),
      alarmHour(6), alarmMinute(30), alarmEnabled(false), alarmRinging(false),
      lastBuzzerToggle(0), buzzerState(false), autoDimmerEnabled(false)
{
    for (int i = 0; i < 6; i++) {
        displayRaw[i] = CHAR_BLANK;
    }
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
void SevenSegmentClock595::begin(uint8_t dataPin, uint8_t clockPin, uint8_t latchPin,
                                 uint8_t buzzerPin, uint8_t dimmerLedPin)
{
    pinData      = dataPin;
    pinClock     = clockPin;
    pinLatch     = latchPin;
    pinBuzzer    = buzzerPin;
    pinDimmerLed = dimmerLedPin;

    pinMode(pinData, OUTPUT);
    pinMode(pinClock, OUTPUT);
    pinMode(pinLatch, OUTPUT);
    digitalWrite(pinLatch, LOW);
    digitalWrite(pinClock, LOW);
    digitalWrite(pinData, LOW);

    pinMode(pinBuzzer, OUTPUT);
    digitalWrite(pinBuzzer, LOW);

    pinMode(pinDimmerLed, OUTPUT);
    digitalWrite(pinDimmerLed, LOW);

    // Default buttons: D5, D6, D7, D8, D9
    setButtons(5, 6, 7, 8, 9);

    // Clear all displays immediately on boot
    updateShiftRegisters(0x00, 0x00, 0x00);

    // Initialize RTC via I2C
    Wire.begin();
    if (rtc.begin()) {
        rtcFound = true;
        if (!rtc.isrunning()) {
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        }
        DateTime now = rtc.now();
        currentHour      = now.hour();
        currentMinute    = now.minute();
        currentSecond    = now.second();
        currentDay       = now.day();
        currentMonth     = now.month();
        currentYear      = now.year();
        currentDayOfWeek = now.dayOfTheWeek();
        loadSettings();
    } else {
        rtcFound = false;
        currentHour      = 12;
        currentMinute    = 0;
        currentSecond    = 0;
        currentDay       = 1;
        currentMonth     = 1;
        currentYear      = 2026;
        currentDayOfWeek = 0;
    }

    updateDisplayData();
    lastSecondTick = millis();
}

void SevenSegmentClock595::setButtons(uint8_t btnMode, uint8_t btnSet, uint8_t btnUp,
                                      uint8_t btnDown, uint8_t btnDimmer)
{
    uint8_t pins[5] = {btnMode, btnSet, btnUp, btnDown, btnDimmer};
    for (int i = 0; i < 5; i++) {
        buttons[i].pin = pins[i];
        buttons[i].lastState = HIGH;
        buttons[i].lastDebounceTime = 0;
        buttons[i].pressStartTime = 0;
        pinMode(pins[i], INPUT_PULLUP);
    }
}

// -------------------------------------------------------------------------
// Main Update Engine (Call inside Arduino loop())
// -------------------------------------------------------------------------
void SevenSegmentClock595::update() {
    unsigned long currentMicros = micros();
    unsigned long currentMillis = millis();

    // 1. Process Button Inputs
    handleButtons();

    // 2. Setting Blink Phase (350ms period)
    if (currentMillis - lastBlinkTime >= 350) {
        lastBlinkTime = currentMillis;
        blinkState = !blinkState;
    }

    // 3. 1-Second Timekeeper Tick
    if (currentMillis - lastSecondTick >= 1000) {
        lastSecondTick += 1000;

        currentSecond++;
        if (currentSecond >= 60) {
            currentSecond = 0;
            currentMinute++;
            if (currentMinute >= 60) {
                currentMinute = 0;
                currentHour = (currentHour + 1) % 24;
            }
        }

        // Periodic RTC synchronization (every 5 seconds)
        if (rtcFound && (currentMillis - lastRtcSyncTime >= 5000)) {
            lastRtcSyncTime = currentMillis;
            DateTime now = rtc.now();
            static int lastObservedSec = -1;

            if (now.second() != lastObservedSec && lastObservedSec != -1) {
                currentHour      = now.hour();
                currentMinute    = now.minute();
                currentSecond    = now.second();
                currentDay       = now.day();
                currentMonth     = now.month();
                currentYear      = now.year();
                currentDayOfWeek = now.dayOfTheWeek();
            }
            lastObservedSec = now.second();
        }

        updateDisplayData();
        checkAlarmTrigger();
    }

    // 4. Alarm Buzzer Sound Routine
    handleAlarmBuzzer();

    // 5. Multiplexing Routine (~67 Hz Refresh Rate)
    if (currentMicros - lastMuxTime >= muxIntervalUs) {
        lastMuxTime = currentMicros;
        multiplexDisplay();
    }
}

// -------------------------------------------------------------------------
// Low-Level 74HC595 Communication
// -------------------------------------------------------------------------
void SevenSegmentClock595::shiftByteFast(uint8_t val) {
    for (int i = 7; i >= 0; i--) {
        digitalWrite(pinData, (val >> i) & 1);
        delayMicroseconds(1);
        digitalWrite(pinClock, HIGH);
        delayMicroseconds(2);
        digitalWrite(pinClock, LOW);
        delayMicroseconds(1);
    }
}

void SevenSegmentClock595::updateShiftRegisters(uint8_t segPattern, uint8_t digitPattern, uint8_t indicatorPattern) {
    digitalWrite(pinLatch, LOW);
    delayMicroseconds(1);
    shiftByteFast(indicatorPattern); // U3 (Furthest chip)
    shiftByteFast(digitPattern);     // U2 (Middle chip)
    shiftByteFast(segPattern);       // U1 (First chip)
    digitalWrite(pinLatch, HIGH);
    delayMicroseconds(2);
    digitalWrite(pinLatch, LOW);
}

void SevenSegmentClock595::multiplexDisplay() {
    currentMuxDigit = (currentMuxDigit + 1) % 6;

    uint8_t segByte = displayRaw[currentMuxDigit] & 0x7F;
    if (colonActive) {
        segByte |= BIT_COLON;
    }

    uint8_t digitByte = DIGIT_MAP[currentMuxDigit];
    updateShiftRegisters(segByte, digitByte, currentIndicatorByte);
}

// -------------------------------------------------------------------------
// Display Formatting & Buffering
// -------------------------------------------------------------------------
void SevenSegmentClock595::setTwoDigits(int startIndex, int value, bool shouldBlink) {
    if (shouldBlink && !blinkState) {
        displayRaw[startIndex]     = CHAR_BLANK;
        displayRaw[startIndex + 1] = CHAR_BLANK;
    } else {
        displayRaw[startIndex]     = SEGMENT_MAP[(value / 10) % 10];
        displayRaw[startIndex + 1] = SEGMENT_MAP[value % 10];
    }
}

void SevenSegmentClock595::updateDisplayData() {
    currentIndicatorByte = (1 << currentDayOfWeek);
    if (alarmEnabled && !alarmRinging) {
        currentIndicatorByte |= BIT_ALARM_LED;
    }

    if (currentMode == MODE_STATUS_MSG) return;

    if (currentMode == MODE_NORMAL) {
        // Show date for the last dateDisplayDuration seconds of each minute (or on manual peek)
        showingDate = (currentSecond >= (60 - dateDisplayDuration)) || (millis() < manualDateTimeout);

        if (!showingDate) {
            setTwoDigits(0, currentHour, false);
            setTwoDigits(2, currentMinute, false);
            setTwoDigits(4, currentSecond, false);
            colonActive = (currentSecond % 2 == 0);
        } else {
            setTwoDigits(0, currentDay, false);
            setTwoDigits(2, currentMonth, false);
            setTwoDigits(4, currentYear % 100, false);
            colonActive = false;
        }
    } else if (currentMode == MODE_SET_TIME) {
        setTwoDigits(0, editHour,   (editField == 0));
        setTwoDigits(2, editMinute, (editField == 1));
        setTwoDigits(4, 0,          false);
        colonActive = true;
    } else if (currentMode == MODE_SET_DATE) {
        setTwoDigits(0, editDay,        (editField == 0));
        setTwoDigits(2, editMonth,      (editField == 1));
        setTwoDigits(4, editYear % 100, (editField == 2));
        colonActive = false;
    } else if (currentMode == MODE_SET_ALARM) {
        displayRaw[0] = CHAR_A;
        displayRaw[1] = CHAR_L;
        setTwoDigits(2, editHour,   (editField == 0));
        setTwoDigits(4, editMinute, (editField == 1));
        colonActive = false;
    }
}

void SevenSegmentClock595::showText(const char* text6) {
    int len = strlen(text6);
    for (int i = 0; i < 6; i++) {
        displayRaw[i] = (i < len) ? getCharPattern(text6[i]) : CHAR_BLANK;
    }
}

void SevenSegmentClock595::setColon(bool active) {
    colonActive = active;
}

void SevenSegmentClock595::setAlarmIndicator(bool state) {
    if (state) currentIndicatorByte |= BIT_ALARM_LED;
    else       currentIndicatorByte &= ~BIT_ALARM_LED;
}

void SevenSegmentClock595::setDayIndicator(uint8_t dayIndex) {
    if (dayIndex > 6) dayIndex = 6;
    currentDayOfWeek = dayIndex;
    currentIndicatorByte = (currentIndicatorByte & BIT_ALARM_LED) | (1 << dayIndex);
}

// -------------------------------------------------------------------------
// Pushbutton Debouncing & State Navigation
// -------------------------------------------------------------------------
bool SevenSegmentClock595::checkButtonPressed(int index) {
    int reading = digitalRead(buttons[index].pin);

    if (reading != buttons[index].lastState) {
        buttons[index].lastDebounceTime = millis();
        buttons[index].lastState = reading;
    }

    if ((millis() - buttons[index].lastDebounceTime) > 35) {
        if (reading == LOW) {
            if (buttons[index].pressStartTime == 0) {
                buttons[index].pressStartTime = millis();
                return true;
            } else {
                if (index == 2 || index == 3) { // Long-press auto-repeat on UP/DOWN
                    unsigned long holdDuration = millis() - buttons[index].pressStartTime;
                    if (holdDuration > 550) {
                        static unsigned long lastRepeat = 0;
                        if (millis() - lastRepeat >= 120) {
                            lastRepeat = millis();
                            return true;
                        }
                    }
                }
            }
        } else {
            buttons[index].pressStartTime = 0;
        }
    }
    return false;
}

void SevenSegmentClock595::showAlarmStatusMsg(bool isOn) {
    currentMode = MODE_STATUS_MSG;
    msgTimeout = millis() + 1500;

    if (isOn) currentIndicatorByte |= BIT_ALARM_LED;
    else      currentIndicatorByte &= ~BIT_ALARM_LED;

    displayRaw[0] = CHAR_A;
    displayRaw[1] = CHAR_L;
    displayRaw[2] = CHAR_BLANK;
    displayRaw[3] = CHAR_BLANK;
    displayRaw[4] = CHAR_O;
    displayRaw[5] = isOn ? CHAR_N : CHAR_F;
    colonActive = false;
}

void SevenSegmentClock595::handleButtons() {
    bool btnMode   = checkButtonPressed(0);
    bool btnSet    = checkButtonPressed(1);
    bool btnUp     = checkButtonPressed(2);
    bool btnDown   = checkButtonPressed(3);
    bool btnDimmer = checkButtonPressed(4);

    // Silencing alarm with any button press
    if (alarmRinging && (btnMode || btnSet || btnUp || btnDown || btnDimmer)) {
        silenceAlarm();
        return;
    }

    // Toggle Dimmer indicator
    if (btnDimmer) {
        autoDimmerEnabled = !autoDimmerEnabled;
        digitalWrite(pinDimmerLed, autoDimmerEnabled ? HIGH : LOW);
        return;
    }

    // MODE transitions
    if (btnMode) {
        if (currentMode == MODE_NORMAL) {
            currentMode = MODE_SET_TIME;
            editHour = currentHour;
            editMinute = currentMinute;
            editField = 0;
        } else if (currentMode == MODE_SET_TIME) {
            currentHour = editHour;
            currentMinute = editMinute;
            currentSecond = 0;
            if (rtcFound) {
                rtc.adjust(DateTime(currentYear, currentMonth, currentDay, currentHour, currentMinute, 0));
            }
            currentMode = MODE_SET_DATE;
            editDay = currentDay;
            editMonth = currentMonth;
            editYear = currentYear;
            editField = 0;
        } else if (currentMode == MODE_SET_DATE) {
            int maxD = getDaysInMonth(editMonth, editYear);
            if (editDay > maxD) editDay = maxD;
            currentDay = editDay;
            currentMonth = editMonth;
            currentYear = editYear;
            if (rtcFound) {
                rtc.adjust(DateTime(currentYear, currentMonth, currentDay, currentHour, currentMinute, currentSecond));
            }
            currentMode = MODE_SET_ALARM;
            editHour = alarmHour;
            editMinute = alarmMinute;
            editField = 0;
        } else if (currentMode == MODE_SET_ALARM) {
            alarmHour = editHour;
            alarmMinute = editMinute;
            alarmEnabled = true;
            currentIndicatorByte |= BIT_ALARM_LED;
            saveSettings();
            currentMode = MODE_NORMAL;
        }
        return;
    }

    // UP / DOWN / SET handling
    switch (currentMode) {
        case MODE_NORMAL:
            if (btnUp) {
                alarmEnabled = !alarmEnabled;
                saveSettings();
                showAlarmStatusMsg(alarmEnabled);
            }
            if (btnDown) {
                manualDateTimeout = millis() + 4000;
            }
            break;

        case MODE_SET_TIME:
            if (btnSet) {
                editField = (editField + 1) % 2;
            }
            if (btnUp) {
                if (editField == 0) editHour = (editHour + 1) % 24;
                else if (editField == 1) editMinute = (editMinute + 1) % 60;
            }
            if (btnDown) {
                if (editField == 0) editHour = (editHour == 0) ? 23 : editHour - 1;
                else if (editField == 1) editMinute = (editMinute == 0) ? 59 : editMinute - 1;
            }
            break;

        case MODE_SET_DATE:
            if (btnSet) {
                editField = (editField + 1) % 3;
            }
            if (btnUp) {
                if (editField == 0) {
                    int maxD = getDaysInMonth(editMonth, editYear);
                    editDay = (editDay % maxD) + 1;
                } else if (editField == 1) {
                    editMonth = (editMonth % 12) + 1;
                    int maxD = getDaysInMonth(editMonth, editYear);
                    if (editDay > maxD) editDay = maxD;
                } else if (editField == 2) {
                    editYear = (editYear >= 2099) ? 2024 : editYear + 1;
                }
            }
            if (btnDown) {
                if (editField == 0) {
                    int maxD = getDaysInMonth(editMonth, editYear);
                    editDay = (editDay <= 1) ? maxD : editDay - 1;
                } else if (editField == 1) {
                    editMonth = (editMonth <= 1) ? 12 : editMonth - 1;
                    int maxD = getDaysInMonth(editMonth, editYear);
                    if (editDay > maxD) editDay = maxD;
                } else if (editField == 2) {
                    editYear = (editYear <= 2024) ? 2099 : editYear - 1;
                }
            }
            break;

        case MODE_SET_ALARM:
            if (btnSet) {
                editField = (editField + 1) % 2;
            }
            if (btnUp) {
                if (editField == 0) editHour = (editHour + 1) % 24;
                else if (editField == 1) editMinute = (editMinute + 1) % 60;
            }
            if (btnDown) {
                if (editField == 0) editHour = (editHour == 0) ? 23 : editHour - 1;
                else if (editField == 1) editMinute = (editMinute == 0) ? 59 : editMinute - 1;
            }
            break;

        case MODE_STATUS_MSG:
            if (millis() > msgTimeout) {
                currentMode = MODE_NORMAL;
            }
            break;
    }
}

// -------------------------------------------------------------------------
// Alarm System
// -------------------------------------------------------------------------
void SevenSegmentClock595::checkAlarmTrigger() {
    if (!alarmEnabled || alarmRinging) return;
    if (currentHour == alarmHour && currentMinute == alarmMinute && currentSecond == 0) {
        alarmRinging = true;
    }
}

void SevenSegmentClock595::handleAlarmBuzzer() {
    if (!alarmRinging) {
        digitalWrite(pinBuzzer, LOW);
        return;
    }

    if (millis() - lastBuzzerToggle >= 200) {
        lastBuzzerToggle = millis();
        buzzerState = !buzzerState;
        digitalWrite(pinBuzzer, buzzerState ? HIGH : LOW);

        if (buzzerState) currentIndicatorByte |= BIT_ALARM_LED;
        else             currentIndicatorByte &= ~BIT_ALARM_LED;
    }
}

void SevenSegmentClock595::silenceAlarm() {
    alarmRinging = false;
    digitalWrite(pinBuzzer, LOW);
    if (alarmEnabled) currentIndicatorByte |= BIT_ALARM_LED;
}

// -------------------------------------------------------------------------
// NVRAM Settings Persistence
// -------------------------------------------------------------------------
void SevenSegmentClock595::saveSettings() {
    if (!rtcFound) return;
    rtc.writenvram(NVRAM_MAGIC_ADDR, MAGIC_BYTE);
    rtc.writenvram(NVRAM_HOUR_ADDR, (uint8_t)alarmHour);
    rtc.writenvram(NVRAM_MIN_ADDR, (uint8_t)alarmMinute);
    rtc.writenvram(NVRAM_EN_ADDR, alarmEnabled ? 1 : 0);
}

void SevenSegmentClock595::loadSettings() {
    if (!rtcFound) return;
    if (rtc.readnvram(NVRAM_MAGIC_ADDR) == MAGIC_BYTE) {
        alarmHour    = rtc.readnvram(NVRAM_HOUR_ADDR);
        alarmMinute  = rtc.readnvram(NVRAM_MIN_ADDR);
        alarmEnabled = (rtc.readnvram(NVRAM_EN_ADDR) == 1);
        if (alarmHour > 23) alarmHour = 6;
        if (alarmMinute > 59) alarmMinute = 0;
    } else {
        alarmHour = 6;
        alarmMinute = 30;
        alarmEnabled = false;
        saveSettings();
    }
}

// -------------------------------------------------------------------------
// Date Calculation Utilities
// -------------------------------------------------------------------------
bool SevenSegmentClock595::isLeapYear(int y) {
    return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

int SevenSegmentClock595::getDaysInMonth(int m, int y) {
    if (m == 2) return isLeapYear(y) ? 29 : 28;
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}

uint8_t SevenSegmentClock595::getCharPattern(char c) {
    switch (c) {
        case '0': return 0b00111111;
        case '1': return 0b00000110;
        case '2': return 0b01011011;
        case '3': return 0b01001111;
        case '4': return 0b01100110;
        case '5': return 0b01101101;
        case '6': return 0b01111101;
        case '7': return 0b00000111;
        case '8': return 0b01111111;
        case '9': return 0b01101111;
        case 'A': case 'a': return 0b01110111;
        case 'B': case 'b': return 0b01111100;
        case 'C':           return 0b00111001;
        case 'c':           return 0b01011000;
        case 'D': case 'd': return 0b01011110;
        case 'E': case 'e': return 0b01111001;
        case 'F': case 'f': return 0b01110001;
        case 'H':           return 0b01110110;
        case 'h':           return 0b01110100;
        case 'L': case 'l': return 0b00111000;
        case 'N': case 'n': return 0b01010100;
        case 'O':           return 0b00111111;
        case 'o':           return 0b01011100;
        case 'P': case 'p': return 0b01110011;
        case 'R': case 'r': return 0b01010000;
        case 'S': case 's': return 0b01101101;
        case 'U':           return 0b00111110;
        case 'u':           return 0b00011100;
        case 'Y': case 'y': return 0b01101110;
        case '-':           return 0b01000000;
        case ' ': default:  return CHAR_BLANK;
    }
}

