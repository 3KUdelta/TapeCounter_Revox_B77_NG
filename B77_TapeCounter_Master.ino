/*
  __ __|                     ___|                       |                    __ ) ___  | ___  |
    |   _` |  __ \    _ \  |       _ \   |   |  __ \   __|   _ \   __|      __ \     /      /
    |  (   |  |   |   __/  |      (   |  |   |  |   |  |     __/  |         |   |   /      /
   _| \__,_|  .__/  \___| \____| \___/  \__,_| _|  _| \__| \___| _|        ____/  _/     _/
             _|

  ARDUINO OLED TAPE COUNTER FOR REVOX B77 — MASTER VERSION 2.1
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  Hardware: Arduino Nano Every (ATmega4809)
  HiFiLabor.ch, Marc Stähli

  Based on:
    B77_TapeCounter v2.0, DIYLAB 2020 / Marc Stähli 2023
    EZS_UNO v1.5, tape length & remaining time logic

  New in v2.1:
    - Mode 2: real-time remaining tape display (replaces meter mode)
    - Automatic tape thickness calibration at band start
    - Merged sensor ISR with EZS ring-buffer reel period tracking
    - Ported restlauf() + lenrout() from EZS for Archimedes-spiral
      remaining-time calculation
    - Nano Every (ATmega4809) target: 6 KB SRAM, no register hacks

  Libraries:
    OneButton   https://github.com/mathertel/OneButton
    Encoder     https://github.com/PaulStoffregen/Encoder
    SSD1306Ascii https://github.com/greiman/SSD1306Ascii
*/

#include "config.h"

#define ENCODER_DO_NOT_USE_INTERRUPTS
#include <OneButton.h>
#include <Encoder.h>
#include <EEPROM.h>
#include <SSD1306Ascii.h>
#include "SSD1306AsciiWire.h"
#include "tapecounter_16x24.h"
#include "tapecounter_21x32.h"

#if HELLO_LOGO
#include "logo_mst1.h"
#endif

// ============================================================
//  Objects
// ============================================================
OneButton         btn(BUTTON, true, true);
Encoder           rotaryEncoder(ROTARY_A, ROTARY_B);
SSD1306AsciiWire  oled;

// ============================================================
//  Global variables
// ============================================================

// --- Operating hours & save trigger -------------------------
volatile uint32_t operatingHoursCounter = 0;
volatile uint32_t saveCounter           = 0;

// --- Speed & display state ----------------------------------
volatile bool  ledTrigger    = false;
volatile bool  speedTrigger  = false;
volatile bool  pauseTrigger  = false;
volatile bool  lockDisplay   = false;
volatile float speedActual   = SPEEDLOW;
volatile float pastSpeedActual = -1.0f;
volatile byte  mode          = MODE_COUNTER;
volatile byte  brightness    = BRIGHTNESS;
volatile byte  loopCounter   = 0;

// --- B77 counters -------------------------------------------
long    counter              = 0;
long    pastCounter          = 0;
long    pastLongCounterSecs  = 0;
float   counterMeters        = 0.0f;
float   counterSeconds       = 0.0f;
float   secsPerPulse         = 0.0f;
float   mmPerPulse           = 0.0f;

// --- Rewind-to-zero -----------------------------------------
bool     rewindToZero    = false;  // procedure active flag
bool     isMoving        = false;  // reel turning (500ms sample)
uint32_t rew2zeroTimeout = 0;      // millis() watchdog
uint8_t  rew2zeroPhase   = 0;      // 0=idle 1=coarse 2=fine 3=done
long     prevCounter     = 0;

// --- Timing intervals (millis-based) ------------------------
uint32_t intervalA = 0;   // 30 ms display refresh
uint32_t intervalB = 0;   // rewind debounce
uint32_t intervalC = 0;   // LED monoflop
uint32_t intervalD = 0;   // speed display timeout
uint32_t intervalE = 0;   // brightness step
uint32_t intervalF = 0;   // stop debounce
uint32_t intervalM = 0;   // movement detection
uint32_t interval5ms = 0; // runmode_b77 tick

// --- Misc ---------------------------------------------------
bool firsttimerun = true;

// Encoder position tracking
long encPos = 0;

// ============================================================
//  setup()
// ============================================================
void setup() {

  pinMode(SENSOR_A,      INPUT);
  pinMode(SENSOR_B,      INPUT);
  pinMode(DIRECTION_PIN, INPUT_PULLUP);  // LOW = B77 REW active
  pinMode(SPEED_PIN,     INPUT);
  pinMode(PAUSE_PIN,     INPUT_PULLUP);
  pinMode(LED,       OUTPUT);
  pinMode(REW_PIN,   OUTPUT);
  pinMode(STOP_PIN,  OUTPUT);

  // Sensor interrupts — Nano Every supports CHANGE on any pin
  attachInterrupt(digitalPinToInterrupt(SENSOR_A), CheckState,  CHANGE);
  attachInterrupt(digitalPinToInterrupt(SENSOR_B), CheckStateB, CHANGE);

  // Button callbacks
  btn.attachClick(ButtonClick);
  btn.attachDoubleClick(ButtonDoubleClick);
  btn.attachLongPressStart(ButtonLongPressStart);
  btn.attachLongPressStop(ButtonLongPressStop);
  btn.attachDuringLongPress(ButtonLongPress);

  // OLED init
  Wire.begin();
  oled.begin(&Adafruit128x32, I2C_ADDRESS);
  oled.setContrast(BRIGHTNESS);
  oled.displayRemap(FLIPDISPLAY);
  oled.invertDisplay(INVERTDISPLAY);
  oled.clear();

#if HELLO_LOGO
  byte a = 0;
  for (byte b = 0; b < 4; b++) {
    oled.setCursor(0, b);
    for (byte i = 0; i < 128; i++) {
      oled.ssd1306WriteRam(pgm_read_byte(&logo[a++]));
    }
  }
  delay(HELLO_TIMEOUT);
  oled.clear();
#endif

  // ---- EEPROM init ------------------------------------------
  // On Nano Every EEPROM is 256 bytes. Use bytes 254-255 as
  // virgin markers (0xFF = unwritten flash default).
  if (EEPROM.read(EE_INIT_A) == 0xFF && EEPROM.read(EE_INIT_B) == 0xFF) {
    EEPROM.write(EE_INIT_A, 0x00);
    EEPROM.write(EE_INIT_B, 0x00);
    // Write defaults
    EEPROM.write(EE_MODE,       MODE_COUNTER);
    EEPROM.write(EE_BRIGHTNESS, BRIGHTNESS);
    EEPROM.put(EE_COUNTER,  (int32_t)0);
    EEPROM.put(EE_SECONDS,  (float)0.0f);
    EEPROM.put(EE_OPHOURS,  (uint32_t)0);
  }

  // Load saved state
  mode       = EEPROM.read(EE_MODE);
  if (mode > MODE_MAX) mode = MODE_COUNTER;
  brightness = EEPROM.read(EE_BRIGHTNESS);
  EEPROM.get(EE_COUNTER,  counter);
  EEPROM.get(EE_SECONDS,  counterSeconds);
  EEPROM.get(EE_OPHOURS,  operatingHoursCounter);

  // Load calibration data for remaining-time calc
  tape_calc_load();

  // Init speed and reel period parameters
  PollingSpeedInputs();
  CalculatingSpeed();
  runmode_b77_init();

  encPos = rotaryEncoder.read();

  // Timer1: 1 Hz for operating hours counter and 1ms sub-tick
  // On Nano Every we use TimerOne library or millis()-based approach.
  // Here we use a simple millis()-based 1s interval in loop()
  // and a separate 1ms millis()-based tick for runmode.
  // (Direct TCCR register access not available on ATmega4809)

  oled.setFont(tapecounter_16x24);
  RefreshDisplay(true);
}

// ============================================================
//  loop()
// ============================================================
void loop() {

  uint32_t now = millis();

  // --- Button & encoder ---------------------------------------
  btn.tick();

  long newPos = rotaryEncoder.read();
  if (newPos != encPos && (newPos % 4 == 0)) {
    EncoderRotated(newPos > encPos ? 1 : 0);
    encPos = newPos;
  }

  // --- Pause detection ----------------------------------------
  if (digitalRead(PAUSE_PIN) == LOW && !pauseTrigger) {
    if (firsttimerun) {
      delay(3000);
      firsttimerun = false;
    }
    oled.clear();
    CenterStringAndWrite("PAUSE", 1);
    pauseTrigger = true;
  } else if (digitalRead(PAUSE_PIN) == HIGH && !lockDisplay) {
    if (pauseTrigger) {
      RefreshDisplay(true);
      pauseTrigger = false;
    }
  }

  // --- 5 ms: runmode + tape calc ------------------------------
  if (now - interval5ms >= 5) {
    interval5ms = now;
    runmode_b77_tick();   // tick all ms-timers (approx, ±5ms)
    runmode_b77();        // moving average + stop detection
    tape_restlauf();      // remaining time (heavy path: 1/rev)
    tape_lenrout();       // absolute position (heavy path: 1/rev)
  }

  // --- 1 s: operating hours -----------------------------------
  // Handled via saveCounter incremented in 1s millis interval
  static uint32_t interval1s = 0;
  if (now - interval1s >= 1000) {
    interval1s = now;
    operatingHoursCounter++;
    saveCounter++;
    if (operatingHoursCounter % 600 == 0) {
      EEPROM.put(EE_OPHOURS, operatingHoursCounter);
    }
  }

  // --- Movement detection (500 ms) ----------------------------
  if (now - intervalM >= 500) {
    intervalM  = now;
    isMoving   = (prevCounter != counter);
    prevCounter = counter;
  }

  // --- Rewind-to-zero -----------------------------------------
  if (rewindToZero) rew2zero_run();

  // --- 30 ms: display refresh ---------------------------------
  if (now - intervalA >= 30) {
    intervalA = now;
    CalculatingDisplayParameters();
    if (!lockDisplay) RefreshDisplay(false);
  }

  // --- LED monoflop -------------------------------------------
  if (ledTrigger && (now - intervalC > 500)) {
    ledTrigger = false;
    digitalWrite(LED, LOW);
  }

  // --- Speed display timeout ----------------------------------
  if (speedTrigger && (now - intervalD > 2000)) {
    speedTrigger = false;
    if (lockDisplay) {
      oled.clear();
      lockDisplay  = false;
      pauseTrigger = false;
    }
  }

  // --- EEPROM save trigger ------------------------------------
  if (saveCounter >= 4) {
    saveCounter++;
    ledTrigger  = true;
    intervalC   = now;
    digitalWrite(LED, HIGH);
    SaveCounter();
  }
}

// ============================================================
//  Transport helpers
// ============================================================
void doStop() {
  if (millis() > intervalF + 250) {
    intervalF = millis();
    digitalWrite(STOP_PIN, HIGH);
    delay(10);
    digitalWrite(STOP_PIN, LOW);
  }
}

void doRewind() {
  if (millis() > intervalB + 1500) {
    intervalB = millis();
    digitalWrite(REW_PIN, HIGH);
    delay(10);
    digitalWrite(REW_PIN, LOW);

// ============================================================
//  Rewind-to-Zero  —  state machine
//
//  Uses timeword (seconds from band start) as position reference,
//  not counter values. This makes the braking points independent
//  of spool size and tape speed.
//
//  Phase 0 (COARSE): rewind full speed until timeword < COARSE_SECS
//  Phase 1 (FINE)  : brief stop, then short rewind burst until
//                    timeword < FINE_SECS (= lockst from EZS)
//  Phase 2 (DONE)  : final stop, display "---", exit
//
//  Safety: 30-second timeout aborts the procedure if the band
//  stops unexpectedly (jam, broken tape, end-of-tape sensor).
// ============================================================

// References to runmode state
extern volatile uint8_t  stopflg;
// References to tape_calc state
extern volatile int16_t timeword;

void rew2zero_start() {
  rewindToZero     = true;
  rew2zeroPhase    = 0;
  rew2zeroTimeout  = millis();
  // Start rewinding immediately — no "movement == still" hack needed.
  // If band is already stopped, doRewind() fires straight away.
  // If band is running (e.g. still in play), it will be called once
  // stopflg is set (band reaches stop on its own or via STOP relay).
  oled.clear();
  CenterStringAndWrite("REW >0", 1);
}

void rew2zero_run() {

  // ---- Abort conditions ---------------------------------------
  if (!rewindToZero) return;

  // Timeout: abort if no progress for REW2ZERO_TIMEOUT_SECS seconds
  if (millis() - rew2zeroTimeout > (uint32_t)REW2ZERO_TIMEOUT_SECS * 1000UL) {
    doStop();
    rewindToZero  = false;
    rew2zeroPhase = 0;
    RefreshDisplay(true);
    return;
  }

  // Already at start (sensor-independent safety catch)
  if (timeword <= REW2ZERO_FINAL_SECS) {
    doStop();
    rewindToZero  = false;
    rew2zeroPhase = 0;
    RefreshDisplay(true);
    return;
  }

  // Reset timeout whenever band is still moving (progress confirmed)
  if (isMoving) rew2zeroTimeout = millis();

  // ---- Phase 0: coarse rewind ---------------------------------
  if (rew2zeroPhase == 0) {
    if (!isMoving && stopflg) {
      // Band has stopped — trigger rewind
      doRewind();
    }
    // Transition to fine phase when close enough
    if (timeword < REW2ZERO_COARSE_SECS) {
      doStop();
      rew2zeroPhase = 1;
      rew2zeroTimeout = millis();  // restart timeout for fine phase
    }
    return;
  }

  // ---- Phase 1: fine rewind -----------------------------------
  if (rew2zeroPhase == 1) {
    // Wait for band to fully stop after coarse stop command
    if (!isMoving && stopflg) {
      doRewind();
    }
    if (timeword < REW2ZERO_FINE_SECS) {
      doStop();
      rew2zeroPhase = 2;
    }
    return;
  }

  // ---- Phase 2: done ------------------------------------------
  if (rew2zeroPhase == 2) {
    // Wait for complete stop, then declare finished
    if (!isMoving && stopflg) {
      rewindToZero  = false;
      rew2zeroPhase = 0;
      RefreshDisplay(true);
    }
  }
}

// ============================================================
//  EEPROM
// ============================================================
void SaveCounter() {
  EEPROM.put(EE_COUNTER,  counter);
  EEPROM.put(EE_SECONDS,  counterSeconds);
  EEPROM.put(EE_OPHOURS,  operatingHoursCounter);
}

// ============================================================
//  Encoder
// ============================================================
void EncoderRotated(bool upsense) {
  if (upsense) {
    if (mode < MODE_MAX) mode++;
    else                 mode = MODE_COUNTER;
  } else {
    if (mode > MODE_COUNTER) mode--;
    else                     mode = MODE_MAX;
  }
  EEPROM.update(EE_MODE, mode);
  oled.clear();
  RefreshDisplay(true);
}

// ============================================================
//  Button callbacks
// ============================================================
void ButtonClick() {
  // Reset all counters
  counter        = 0;
  counterMeters  = 0.0f;
  counterSeconds = 0.0f;
  sensorIsr_resetCounters();
  // Reset tape calc state (band start assumed)
  timeword = 0;
  abstime  = 0;
  resttime = 0;
  SaveCounter();
  oled.clear();
  RefreshDisplay(true);
}

void ButtonDoubleClick() {
  if (counter > 0 || timeword > REW2ZERO_MIN_SECS) {
    rew2zero_start();
  }
}

void ButtonLongPress() {
  if (millis() > intervalE + 300) {
    intervalE = millis();
    brightness++;
    if (brightness > 16) brightness = 0;
    char buf[17];
    oled.setCursor(75 + OLED_X_OFFSET, 2 + OLED_Y_OFFSET);
    for (size_t i = 0; i < 16; i++)
      buf[i] = (i < brightness) ? 133 : 32;
    buf[16] = 0;
    oled.print(buf);
    oled.setContrast(brightness * 16);
  }
}

void ButtonLongPressStart() {
  lockDisplay = true;
  oled.setFont(utf8font10x16);
  oled.clear();
  oled.setCursor(0 + OLED_X_OFFSET, 2 + OLED_Y_OFFSET);
  oled.println("BRIGHTNESS:");
}

void ButtonLongPressStop() {
  EEPROM.update(EE_BRIGHTNESS, brightness);
  for (size_t i = 0; i < 2; i++) {
    oled.setCursor(75 + OLED_X_OFFSET, 2 + OLED_Y_OFFSET);
    oled.print("STORED      ");
    delay(250);
    oled.setCursor(75 + OLED_X_OFFSET, 2 + OLED_Y_OFFSET);
    oled.print("            ");
    delay(250);
  }
  oled.setFont(tapecounter_16x24);
  oled.clear();
  lockDisplay = false;
  RefreshDisplay(true);
}

// ============================================================
//  Display
// ============================================================
void RefreshDisplay(boolean force) {

  long longSecs = (long)counterSeconds;

  OverflowCalculation();

  if (force) {
    switch (mode) {
      case MODE_COUNTER:   WriteOled(counter);     break;
      case MODE_ELAPSED:   WriteOledRealTime(longSecs); break;
      case MODE_REMAINING: WriteOledRemaining();   break;
    }
  } else {
    switch (mode) {
      case MODE_COUNTER:
        if (counter != pastCounter) {
          pastCounter = counter;
          WriteOled(counter);
        }
        break;
      case MODE_ELAPSED:
        if (longSecs != pastLongCounterSecs) {
          pastLongCounterSecs = longSecs;
          WriteOledRealTime(longSecs);
        }
        break;
      case MODE_REMAINING:
        // Remaining time changes every second; always refresh
        WriteOledRemaining();
        break;
    }
  }
}

// --- Counter display -----------------------------------------
void WriteOled(long val) {
  char buf[7];
  oled.setLetterSpacing(4);
  sprintf(buf, (val < 0) ? "-%05ld" : " %05ld",
          (val > 0) ? val : -val);
  oled.setCursor(4 + OLED_X_OFFSET, 1 + OLED_Y_OFFSET);
  oled.println(buf);
}

// --- Elapsed time display ------------------------------------
void WriteOledRealTime(long secs) {
  char buf[8];
  (secs < 0) ? SecondsToString(buf, secs, "-%03d'%02d")
             : SecondsToString(buf, secs, " %03d'%02d");
  oled.setLetterSpacing(4);
  oled.setCursor(5 + OLED_X_OFFSET, 1 + OLED_Y_OFFSET);
  oled.println(buf);
}

// --- Remaining time display (mode 2) -------------------------
void WriteOledRemaining() {
  char buf[10];
  tape_calc_getRemainingStr(buf);
  oled.setLetterSpacing(4);
  oled.setCursor(5 + OLED_X_OFFSET, 1 + OLED_Y_OFFSET);
  oled.println(buf);
}

// ============================================================
//  Helpers
// ============================================================
void SecondsToString(char* buffer, long secs, char* format) {
  secs = (secs > 0) ? secs : -secs;
  int seconds = numberOfSeconds(secs);
  int minutes = numberOfMinutesEx(secs);
  sprintf(buffer, format, minutes, seconds);
}

void CenterStringAndWrite(char* msg, byte row) {
  size_t strWidth = oled.strWidth(msg);
  oled.setCursor(((128 - strWidth) / 2) + OLED_X_OFFSET,
                 row + OLED_Y_OFFSET);
  oled.println(msg);
}

void OverflowCalculation() {
  long tempCounter = abs(counter);
  if (tempCounter > 99999) counter = 0;
  if (counterSeconds < -35999 - secsPerPulse ||
      counterSeconds >  35999 + secsPerPulse)
    counterSeconds = 0;
}

// ============================================================
//  Speed
// ============================================================
void CalculatingDisplayParameters() {
  PollingSpeedInputs();
  CalculatingSpeed();

  if (speedActual != pastSpeedActual) {
    speedTrigger   = true;
    intervalD      = millis();
    pastSpeedActual = speedActual;
    lockDisplay    = true;
    oled.clear();
    oled.setLetterSpacing(2);
    oled.setCursor(0 + OLED_X_OFFSET, 1 + OLED_Y_OFFSET);
    if (speedActual == SPEEDLOW)
      CenterStringAndWrite("3,75 LS", 1);
    else
      CenterStringAndWrite("7,5 STD", 1);
  }
}

void PollingSpeedInputs() {
  speedActual = (digitalRead(SPEED_PIN) == HIGH) ? SPEEDMIDDLE : SPEEDLOW;
}

void CalculatingSpeed() {
  // secsPerPulse based on SPEEDLOW (constant, speed-independent base)
  secsPerPulse = (float)SCOPE / (SPEEDLOW * (float)NUMSEGS);
  secsPerPulse = secsPerPulse * SPEEDLOW / speedActual;
  mmPerPulse   = secsPerPulse * speedActual / 1000.0f;
}
