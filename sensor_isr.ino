// ============================================================
//  sensor_isr.ino
//  Dual-reel sensor interrupt service routines — Option A
//
//  Hardware layout (Option A):
//    SENSOR_A (pin 3) → RIGHT reel  (supply reel at band start)
//    SENSOR_B (pin 2) → LEFT  reel  (takeup reel at band start)
//    DIRECTION_PIN (pin 4) → B77 REW-signal  LOW=rewind, HIGH=forward
//
//  Each reel has one HW-006 line tracker module with a white
//  segment strip (NUMSEGS=4 segments per revolution).
//
//  Ring buffer layout (depth = NUMSEGS):
//    tright[0] / tleft[0]  = accumulator: ms elapsed in current
//                            partial revolution (ticked in tick())
//    tright[1..NUMSEGS]    = completed revolution periods [ms]
//    trightx / tleftx      = write index into the ring (1..NUMSEGS)
//
//  Debounce:
//    tlkurz / trkurz are incremented every ms in runmode_b77_tick().
//    On the FALLING edge of each sensor we reset them to 0.
//    A rising edge is accepted only when the debounce timer >= 1 ms,
//    suppressing spurious re-triggers from sensor noise.
//
//  Direction:
//    dirForward is read once per accepted pulse from DIRECTION_PIN.
//    LOW  = REW active → band moving backward (right reel emptying)
//    HIGH = forward    → PLAY or FFWD (right reel filling)
//    The pin is INPUT_PULLUP so it defaults HIGH (forward) when
//    the B77 is not actively rewinding.
// ============================================================

#include "config.h"

// ------------------------------------------------------------
//  Variables owned by this module
//  All volatile: written in ISR, read in main loop / runmode.
// ------------------------------------------------------------

// Reel revolution ring buffers
volatile int16_t tleft [NUMSEGS + 1];   // left  reel periods [ms]
volatile int16_t tright[NUMSEGS + 1];   // right reel periods [ms]

// Ring buffer write indices (1..NUMSEGS, wrap at NUMSEGS+1 -> 1)
volatile uint8_t tleftx  = 1;
volatile uint8_t trightx = 1;

// Moving averages -- computed in runmode_b77(), read in tape_calc
volatile int16_t tleftmw  = 0;
volatile int16_t trightmw = 0;
volatile int16_t time0    = 0;    // (tleftmw + trightmw) / 2

// Revolution counters (signed, relative to last reset)
//   umrabs: right reel -- positive = forward (filling up during PLAY)
//   umlabs: left  reel -- positive = forward (emptying during PLAY)
volatile int16_t umrabs = 0;
volatile int16_t umlabs = 0;

// runflg: incremented by any sensor pulse; consumed in runmode_b77()
volatile uint8_t runflg = 0;

// Debounce timers -- incremented in runmode_b77_tick() every ms,
// reset to 0 on the falling edge of the respective sensor pin.
volatile uint16_t tlkurz = 0;   // right reel debounce [ms]
volatile uint16_t trkurz = 0;   // left  reel debounce [ms]

// Internal edge-tracking state (one per sensor)
static volatile bool pastPinA = false;
static volatile bool pastPinB = false;

// ------------------------------------------------------------
//  References to variables defined in other modules
// ------------------------------------------------------------
extern volatile float counterSeconds;
extern volatile float counterMeters;
extern long           counter;
extern volatile byte  loopCounter;
extern float          secsPerPulse;
extern float          mmPerPulse;

// ------------------------------------------------------------
//  CheckState()  --  ISR for SENSOR_A (RIGHT reel, pin 3)
//
//  Called on every CHANGE of SENSOR_A.
//  Falling edge: reset tlkurz (start debounce window).
//  Rising edge:  if tlkurz >= 1 ms, accept pulse:
//    - rotate tright[] ring buffer, reset accumulator
//    - update umrabs (right reel revolution count)
//    - update B77 counter / counterSeconds / counterMeters
// ------------------------------------------------------------
void CheckState() {

  bool pinA = digitalRead(SENSOR_A);

  // ---- Falling edge: start debounce window ------------------
  if (pastPinA == HIGH && pinA == LOW) {
    tlkurz = 0;
  }

  // ---- Rising edge: evaluate pulse --------------------------
  if (pastPinA == LOW && pinA == HIGH) {

    // Direction from dedicated pin: LOW = B77 REW active
    bool dirForward = (digitalRead(DIRECTION_PIN) == HIGH);

    if (tlkurz >= 1) {

      // --- Right reel ring buffer ----------------------------
      if (trightx > NUMSEGS) trightx = 1;
      tright[trightx++] = tright[0];
      tright[0] = 0;

      // --- Right reel revolution counter ---------------------
      if (dirForward) umrabs++;
      else            umrabs--;

      // --- B77 display counter (every PULSES_COUNTER pulses) -
      if (loopCounter >= PULSES_COUNTER) {
        loopCounter = 0;
        if (dirForward) counter++;
        else            counter--;
      }
      loopCounter++;

      // --- Continuous time / length accumulators -------------
      if (dirForward) {
        counterMeters  += mmPerPulse;
        counterSeconds += secsPerPulse;
      } else {
        counterMeters  -= mmPerPulse;
        counterSeconds -= secsPerPulse;
      }

      runflg++;
    }
  }

  pastPinA = pinA;
}

// ------------------------------------------------------------
//  CheckStateB()  --  ISR for SENSOR_B (LEFT reel, pin 2)
//
//  Called on every CHANGE of SENSOR_B.
//  Falling edge: reset trkurz (start debounce window).
//  Rising edge:  if trkurz >= 1 ms, accept pulse:
//    - rotate tleft[] ring buffer, reset accumulator
//    - update umlabs (left reel revolution count)
//
//  The left reel does NOT drive the B77 display counter.
//  Its data feeds tape_restlauf() exclusively via tleftmw / umlabs.
// ------------------------------------------------------------
void CheckStateB() {

  bool pinB = digitalRead(SENSOR_B);

  // ---- Falling edge: start debounce window ------------------
  if (pastPinB == HIGH && pinB == LOW) {
    trkurz = 0;
  }

  // ---- Rising edge: evaluate pulse --------------------------
  if (pastPinB == LOW && pinB == HIGH) {

    bool dirForward = (digitalRead(DIRECTION_PIN) == HIGH);

    if (trkurz >= 1) {

      // --- Left reel ring buffer -----------------------------
      if (tleftx > NUMSEGS) tleftx = 1;
      tleft[tleftx++] = tleft[0];
      tleft[0] = 0;

      // --- Left reel revolution counter ----------------------
      // Forward PLAY: left reel pays out tape (radius shrinks).
      // umlabs++ = one more revolution of band leaving the left reel.
      if (dirForward) umlabs++;
      else            umlabs--;

      runflg++;
    }
  }

  pastPinB = pinB;
}

// ------------------------------------------------------------
//  sensorIsr_resetCounters()
//  Call from ButtonClick() to reset all counters to zero.
// ------------------------------------------------------------
void sensorIsr_resetCounters() {
  noInterrupts();
  umrabs  = 0;
  umlabs  = 0;
  tleftx  = 1;
  trightx = 1;
  for (uint8_t i = 0; i <= NUMSEGS; i++) {
    tleft[i]  = 0;
    tright[i] = 0;
  }
  tleftmw  = 0;
  trightmw = 0;
  time0    = 0;
  runflg   = 0;
  tlkurz   = 0;
  trkurz   = 0;
  interrupts();
}
