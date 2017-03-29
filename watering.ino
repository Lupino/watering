#include <stdio.h>
#include <DS1302.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <SimpleTimer.h>

SimpleTimer timer;

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x3f, 16, 2);

DS1302 rtc(8, 7, 6);

const int BUTTON_1 = 2;
const int BUTTON_2 = 3;

const int OUTPUT_PORT_1 = 4;
const int OUTPUT_PORT_2 = 5;
const int OUTPUT_PORT_3 = 10;
const int OUTPUT_PORT_4 = 11;
// const int OUTPUT_PORT_5 = 13;

const int TOTAL_PORT = 4;
int OUTPUT_PINS[TOTAL_PORT] = {OUTPUT_PORT_1, OUTPUT_PORT_2, OUTPUT_PORT_3, OUTPUT_PORT_4/*, OUTPUT_PORT_5*/};
const boolean RELAY_RUN = LOW;
const boolean RELAY_NORUN = HIGH;
boolean RELAY_STATE[TOTAL_PORT];

boolean lastButton1 = LOW;
boolean currentButton1 = LOW;
boolean lastButton2 = LOW;
boolean currentButton2 = LOW;

const float timeout = 60.0;
const int TOTAL_JOBS = 20;
boolean forcePrintTime = false;

int menuType = 0;
Time currentTime = rtc.time();
Time cacheTime = currentTime;

int backlightTimer = -1;

struct Job {
    long duration; // running time second
    long schedAt; // schedule at second

    byte port; // the output port

    int repeat; // every day, once
    boolean running;
    boolean enable; // enable or disable
};

int select;
boolean rerender;
float waiting;
int eeAddress;
Job job;
Time::Day day;
boolean ports[TOTAL_PORT];
long current;
int step;
char line[16];
long remain;
int hour;
int minute;
int second;
int duration_hour;
int duration_minute;
int duration_second;
String dayStr;
int i;
int count;
int type;
boolean isChanged;
boolean isChangedMinute;
boolean isChangedSecond;
int jobID;
boolean currentState;

void initLCD();
void initRelay();
void initTime();
void checkAndRunJobs();
void readAndPrintTime();

String dayAsString(const Time::Day day) {
    switch (day) {
        case Time::kSunday: return F("Sun");
        case Time::kMonday: return F("Mon");
        case Time::kTuesday: return F("Tue");
        case Time::kWednesday: return F("Wed");
        case Time::kThursday: return F("Thu");
        case Time::kFriday: return F("Fri");
        case Time::kSaturday: return F("Sat");
    }
    return F("err");
}

boolean backToMenu() {
    count = 0;
    while (count < 10) {
        delay(100);
        currentButton1 = debounce(BUTTON_1, lastButton1);
        if (currentButton1 == LOW) {
            break;
        }
        count += 1;
    }
    lastButton1 = currentButton1;
    if (currentButton1 == HIGH) {
        return true;
    }

    return false;
}

void printTime(Time nextTime, boolean force) {
    if (!force && nextTime.min == cacheTime.min) {
        if (nextTime.sec != cacheTime.min) {
            lcd.setCursor(6, 1);
            if (nextTime.sec < 10) {
                lcd.print(F("0"));
                lcd.print(nextTime.sec);
            } else {
                lcd.print(nextTime.sec);
            }
            cacheTime = nextTime;
        }
        return;
    }
    // Name the day of the week.
    dayStr = dayAsString(nextTime.day);

    // Format the time and date and insert into the temporary buffer.
    // Print the formatted string to serial so we can see the time.
    snprintf(line, sizeof(line), "%04d-%02d-%02d %s", nextTime.yr, nextTime.mon, nextTime.date, dayStr.c_str());
    lcd.setCursor(0, 0);
    lcd.print(line);

    snprintf(line, sizeof(line), "%02d:%02d:%02d", nextTime.hr, nextTime.min, nextTime.sec);
    lcd.setCursor(0, 1);
    lcd.print(line);
    cacheTime = nextTime;
}

void setup() {
    initLCD();

    initRelay();
    initTime();

    // Initialize a new chip by turning off write protection and clearing the
    // clock halt flag. These methods needn't always be called. See the DS1302
    // datasheet for details.
    rtc.writeProtect(false);
    rtc.halt(false);

    pinMode(BUTTON_1, INPUT);
    pinMode(BUTTON_2, INPUT);

    timer.setInterval(1000, checkAndRunJobs);
    timer.setInterval(250, readAndPrintTime);
}

boolean debounce(int BUTTON, boolean last) {
    currentState = digitalRead(BUTTON);
    if (last != currentState) {
        delay(5);
        currentState = digitalRead(BUTTON);
    }

    return currentState;
}

void settingTime(Time t) {
    // lcd.cursor();
    lcd.blink();
    type = 0;
    isChanged = false;
    isChangedMinute = false;
    isChangedSecond = false;
    rerender = true;
    waiting = 0;
    while (1) {
        waiting += 0.1;
        if (waiting > timeout) {
           break;
        }
        if (rerender) {
            rerender = false;
            printTime(t, true);
            switch(type) {
            case 0: // year
                lcd.setCursor(0, 0);
                break;
            case 1: // month
                lcd.setCursor(5, 0);
                break;
            case 2: // date
                lcd.setCursor(8, 0);
                break;
            case 3: // day
                lcd.setCursor(11, 0);
                break;
            case 4: // hour
                lcd.setCursor(0, 1);
                break;
            case 5: // minute
                lcd.setCursor(3, 1);
                break;
            case 6: // second
                lcd.setCursor(6, 1);
                break;
            }
        }
        currentButton2 = debounce(BUTTON_2, lastButton2);
        if (lastButton2 == LOW && currentButton2 == HIGH) {
            rerender = true;
            waiting = 0;
            isChanged = true;
            switch(type) {
            case 0: // year
                t.yr += 1;
                if (t.yr > 2030) {
                    t.yr = 2015;
                }
                break;
            case 1: // month
                t.mon += 1;
                if (t.mon > 12) {
                    t.mon = 1;
                }
                break;
            case 2: // date
                t.date += 1;
                switch (t.mon) {
                case 1:
                case 3:
                case 5:
                case 7:
                case 8:
                case 10:
                case 12:
                    if (t.date > 31) {
                        t.date = 1;
                    }
                    break;
                case 4:
                case 6:
                case 9:
                case 11:
                    if (t.date > 30) {
                        t.date = 1;
                    }
                    break;
                case 2:
                    if (t.yr % 400 == 0 || (t.yr % 4 == 0 && t.yr % 100 != 0)) {
                        if (t.date > 29) {
                            t.date = 1;
                        }
                    } else {
                        if (t.date > 28) {
                            t.date = 1;
                        }
                    }
                    break;
                }
                break;
            case 3: // day
                switch (t.day) {
                    case Time::kSunday:
                        t.day = Time::kMonday;
                        break;
                    case Time::kMonday:
                        t.day = Time::kTuesday;
                        break;
                    case Time::kTuesday:
                        t.day = Time::kWednesday;
                        break;
                    case Time::kWednesday:
                        t.day = Time::kThursday;
                        break;
                    case Time::kThursday:
                        t.day = Time::kFriday;
                        break;
                    case Time::kFriday:
                        t.day = Time::kSaturday;
                        break;
                    case Time::kSaturday:
                        t.day = Time::kSunday;
                        break;
                }
                break;
            case 4: // hour
                t.hr += 1;
                if (t.hr > 23) {
                    t.hr = 0;
                }
                break;
            case 5: // minute
                isChangedMinute = true;
                t.min += 1;
                if (t.min > 59) {
                    t.min = 0;
                }
                break;
            case 6: // second
                isChangedSecond = true;
                t.sec += 1;
                if (t.sec > 59) {
                    t.sec = 0;
                }
                break;
            }
        }
        lastButton2 = currentButton2;

        currentButton1 = debounce(BUTTON_1, lastButton1);
        if (lastButton1 == LOW && currentButton1 == HIGH) {
            waiting = 0;
            rerender = true;
            type += 1;
            if (type > 5 || backToMenu()) {
                lastButton1 = currentButton1;
                break;
            }
        }
        lastButton1 = currentButton1;

        Time currentTime = rtc.time();
        if (!isChangedMinute) {
            t.min = currentTime.min;
        }
        if (!isChangedSecond) {
            if (t.sec != currentTime.sec) {
                rerender = true;
            }
            t.sec = currentTime.sec;
        }

        delay(100);
    }
    lcd.noBlink();
    if (isChanged) {
        Time currentTime = rtc.time();
        if (!isChangedMinute) {
            t.min = currentTime.min;
        }
        if (!isChangedSecond) {
            t.sec = currentTime.sec;
        }
        rtc.time(t);
    }
    menuType = 0;
}

void printJobs() {
    lcd.clear();
    lcd.setCursor(0, 0);
    jobID = 0;
    eeAddress = 0;
    rerender = true;

    waiting = 0;
    while (1) {
        waiting += 0.1;
        if (waiting > timeout) {
           break;
        }

        if (rerender) {
            waiting = 0;
            rerender = false;
            EEPROM.get(eeAddress, job);

            hour = job.schedAt / 3600;
            remain = job.schedAt % 3600;
            minute = remain / 60;
            second = remain % 60;
            snprintf(line, sizeof(line), "#%02d %02d:%02d:%02d", jobID, hour, minute, second);
            lcd.setCursor(0, 0);
            lcd.print(line);
            lcd.setCursor(13, 0);
            if (job.enable) {
                lcd.print(F(" on"));
            } else {
                lcd.print(F("off"));
            }

            hour = job.duration / 3600;
            remain = job.duration % 3600;
            minute = remain / 60;
            second = remain % 60;
            snprintf(line, sizeof(line), "durat: %02d:%02d:%02d", hour, minute, second);
            lcd.setCursor(0, 1);
            lcd.print(line);
        }

        currentButton2 = debounce(BUTTON_2, lastButton2);
        if (lastButton2 == LOW && currentButton2 == HIGH) {
            rerender = true;
            eeAddress += sizeof(job);
            jobID += 1;
            if (jobID > TOTAL_JOBS) {
                jobID = 0;
                eeAddress = 0;
            }
        }
        lastButton2 = currentButton2;

        currentButton1 = debounce(BUTTON_1, lastButton1);
        if (lastButton1 == LOW && currentButton1 == HIGH) {

            if (backToMenu()) {
                break;
            }

            editJob(jobID, eeAddress);
            rerender = true;
        }
        lastButton1 = currentButton1;
        delay(100);
    }
    menuType = 0;
    lcd.clear();
}

void editJob(int jobID, int eeAddress) {
    EEPROM.get(eeAddress, job);
    rerender = true;
    step = 0;
    lcd.blink();
    waiting = 0;
    hour = job.schedAt / 3600;
    remain = job.schedAt % 3600;
    minute = remain / 60;
    second = remain % 60;

    duration_hour = job.duration / 3600;
    remain = job.duration % 3600;
    duration_minute = remain / 60;
    duration_second = remain % 60;
    while (1) {
        waiting += 0.1;
        if (waiting > timeout) {
           break;
        }
        if (rerender) {
            waiting = 0;
            rerender = false;
            lcd.clear();
            switch (step) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                    snprintf(line, sizeof(line), "#%02d %02d:%02d:%02d", jobID, hour, minute, second);
                    lcd.setCursor(0, 0);
                    lcd.print(line);
                    lcd.setCursor(13, 0);
                    if (job.enable) {
                        lcd.print(F(" on"));
                    } else {
                        lcd.print(F("off"));
                    }

                    snprintf(line, sizeof(line), "durat: %02d:%02d:%02d", duration_hour, duration_minute, duration_second);
                    lcd.setCursor(0, 1);
                    lcd.print(line);
                    break;
                case 7:
                case 8:
                    lcd.setCursor(0, 0);
                    lcd.print(F("repeat:"));
                    lcd.setCursor(13, 0);
                    switch (job.repeat) {
                    case 0:
                        lcd.print(F(" on"));
                        break;
                    case 8:
                        lcd.print(F("off"));
                        break;
                    default:
                        day = static_cast<Time::Day>(job.repeat);
                        dayStr = dayAsString(day);
                        lcd.print(dayStr.c_str());
                        break;
                    }
                    lcd.setCursor(0, 1);
                    lcd.print(F("port: "));
                    lcd.print(job.port + 1);
                    break;
            }
            switch (step) {
            case 0: // hour
                lcd.setCursor(4, 0);
                break;
            case 1: // minute
                lcd.setCursor(7, 0);
                break;
            case 2: // second
                lcd.setCursor(10, 0);
                break;
            case 3: // enable
                lcd.setCursor(13, 0);
                break;
            case 4: // duration hour
                lcd.setCursor(7, 1);
                break;
            case 5: // duration minute
                lcd.setCursor(10, 1);
                break;
            case 6: // duration second
                lcd.setCursor(13, 1);
                break;
            case 7: // repeat
                lcd.setCursor(13, 0);
                break;
            case 8: // port
                lcd.setCursor(6, 1);
                break;
            }
        }

        currentButton2 = debounce(BUTTON_2, lastButton2);
        if (lastButton2 == LOW && currentButton2 == HIGH) {
            rerender = true;
            switch (step) {
            case 0: // hour
                hour += 1;
                if (hour > 23) {
                    hour = 0;
                }
                break;
            case 1: // minute
                minute += 1;
                if (minute > 59) {
                    minute = 0;
                }
                break;
            case 2: // second
                second += 1;
                if (second > 59) {
                    second = 0;
                }
                break;
            case 3: // enable
                job.enable = !job.enable;
                break;
            case 4: // duration hour
                duration_hour += 1;
                if (duration_hour > 12) {
                    duration_hour = 0;
                }
                break;
            case 5: // duration minute
                duration_minute += 1;
                if (duration_minute > 59) {
                    duration_minute = 0;
                }
                break;
            case 6: // duration second
                duration_second += 1;
                if (duration_second > 59) {
                    duration_second = 0;
                }
                break;
            case 7: // repeat
                job.repeat += 1;
                if (job.repeat > 8) {
                    job.repeat = 0;
                }
                break;
            case 8: // port
                job.port += 1;
                if (job.port >= TOTAL_PORT) {
                    job.port = 0;
                }
                break;
            }
        }
        lastButton2 = currentButton2;

        currentButton1 = debounce(BUTTON_1, lastButton1);
        if (lastButton1 == LOW && currentButton1 == HIGH) {
            rerender = true;
            step += 1;
            if (step > 8 || backToMenu()) {
                break;
            }
        }
        lastButton1 = currentButton1;
        delay(100);
    }
    lcd.noBlink();

    job.schedAt = long(hour) * 3600 + long(minute) * 60 + long(second);
    job.duration = long(duration_hour) * 3600 + long(duration_minute) * 60 + long(duration_second);
    EEPROM.put(eeAddress, job);
}

void resetJobs() {
    job.duration = 0; // running time second
    job.schedAt = 0; // schedule at second
    job.port = 0; // the output port
    job.repeat = 8; // every day, once
    job.running = false;
    job.enable = false; // enable or disable

    eeAddress = 0;
    for (i=0; i<=TOTAL_JOBS; i++) {
        EEPROM.put(eeAddress, job);
        eeAddress += sizeof(job);
    }
    menuType = 0;
}

void initRelay() {
    for (i=0;i<TOTAL_PORT;i++) {
        pinMode(OUTPUT_PINS[i], OUTPUT);
        digitalWrite(OUTPUT_PINS[i], RELAY_NORUN);
        RELAY_STATE[i] = RELAY_NORUN;
    }
}

void initLCD() {
    // initialize the LCD
    lcd.begin();
    // Turn on the blacklight and print a message.
    lcd.backlight();
    lcd.print(F("Starting..."));
    backlightTimer = timer.setTimeout(10000, closeLCDBacklight);
}

void initTime() {
    currentTime = rtc.time();
    printTime(currentTime, true);
}

void checkAndRunJobs() {
    eeAddress = 0;
    for (i=0;i<TOTAL_PORT;i++) {
        ports[i] = RELAY_NORUN;
    }

    current = long(currentTime.hr) * 3600 + long(currentTime.min) * 60 + long(currentTime.sec);
    for (i=0; i<=TOTAL_JOBS; i++) {
        EEPROM.get(eeAddress, job);
        eeAddress += sizeof(job);
        if (!job.enable) {
            continue;
        }

        if (job.repeat > 0 && job.repeat < 8) {
            day = static_cast<Time::Day>(job.repeat);
            if (day != currentTime.day) {
                continue;
            }
        }

        if (job.schedAt <= current && current <= job.schedAt + job.duration) {
            ports[job.port] = RELAY_RUN;
        }

        if (job.repeat == 8) {
            if (RELAY_STATE[job.port] == RELAY_RUN && ports[job.port] == RELAY_NORUN) {
                job.enable = false;
                EEPROM.put(eeAddress, job);
            }
        }
    }


    for (i=0;i<TOTAL_PORT;i++) {
        if (RELAY_STATE[i] != ports[i]) {
            digitalWrite(OUTPUT_PINS[i], ports[i]);
            RELAY_STATE[i] = ports[i];
        }
    }
}

void readAndPrintTime() {
    currentTime = rtc.time();
    if (menuType == 0) {
        printTime(currentTime, forcePrintTime);
        forcePrintTime = false;
    }
}

int showMenu() {
    select = 0;
    rerender = true;
    waiting = 0;
    lcd.blink();
    while (1) {
        waiting += 0.1;
        if (waiting > timeout) {
           break;
        }
        if (rerender) {
            waiting = 0;
            rerender = false;
            switch (select) {
            case 0:
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(F("1. show time"));
                lcd.setCursor(0, 1);
                lcd.print(F("2. config time"));
                lcd.setCursor(0, 0);
                break;
            case 1:
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(F("2. config time"));
                lcd.setCursor(0, 1);
                lcd.print(F("3. show jobs"));
                lcd.setCursor(0, 0);
                break;
            case 2:
            case 3:
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print(F("3. show jobs"));
                lcd.setCursor(0, 1);
                lcd.print(F("4. reset"));
                lcd.setCursor(0, select % 2);
                break;
            }
        }
        currentButton2 = debounce(BUTTON_2, lastButton2);
        if (lastButton2 == LOW && currentButton2 == HIGH) {
            rerender = true;
            select += 1;
            if (select > 3) {
                select = 0;
            }
        }
        lastButton2 = currentButton2;

        currentButton1 = debounce(BUTTON_1, lastButton1);
        if (lastButton1 == LOW && currentButton1 == HIGH) {
            lastButton1 = currentButton1;
            break;
        }
        lastButton1 = currentButton1;
        delay(100);
    }
    lcd.noBlink();
    lcd.clear();
    return select;
}

void closeLCDBacklight() {
    lcd.noBacklight();
}

// Loop and print the time every second.
void loop() {
    timer.run();
    switch (menuType) {
    case 1:
        timer.deleteTimer(backlightTimer);
        settingTime(currentTime);
        backlightTimer = timer.setTimeout(10000, closeLCDBacklight);
        break;
    case 2:
        timer.deleteTimer(backlightTimer);
        printJobs();
        backlightTimer = timer.setTimeout(10000, closeLCDBacklight);
        break;
    case 3:
        resetJobs();
        break;
    }

    currentButton1 = debounce(BUTTON_1, lastButton1);
    if (lastButton1 == LOW && currentButton1 == HIGH) {
        lastButton1 = currentButton1;
        lcd.backlight();
        timer.deleteTimer(backlightTimer);
        menuType = showMenu();
        backlightTimer = timer.setTimeout(10000, closeLCDBacklight);
        forcePrintTime = true;
    }
    lastButton1 = currentButton1;
    delay(100);
}
