// ============================================================
//  runmode.ino
//  5 ms periodic task: reel period moving average + STOP detection
//
//  Port of EZS runmode() adapted for B77 Master:
//    - No relay control (B77 handles transport natively)
//    - No playspeed detection needed (only 2 speeds, read from SPEED_PIN)
//    - stopflg: 0 = band running, 255 = band stopped
//    - Feeds tleftmw, trightmw, time0 to tape_calc.ino
// ============================================================

#include "config.h"

// ------------------------------------------------------------
//  Exported state
// ------------------------------------------------------------
volatile uint8_t  stopflg   = 255;  // 255 = stopped (init: assume stopped)
volatile uint16_t stoptime  = 0;    // ms since last pulse (counts up)
volatile uint16_t disptime  = 0;    // ms since band started moving
volatile uint8_t  sync      = 0;    // revolutions since last speed change
                                    // sync >= SYNCKST = "in sync" for calc

#define SYNCKST  1                  // revolutions needed before calc is valid

// Derived limits (set in runmode_b77_init, depend on NUMSEGS)
uint16_t dlytime  = DLYTIME_BASE  / NUMSEGS;
uint16_t stoptime_init = STOPTIME_BASE / NUMSEGS;

// Exported: current detected reel size
volatile uint8_t  spule    = 26;    // 26, 22 or 18 cm
volatile int16_t  midtime  = TMID26;
volatile int16_t  mintime  = TMIN26;
volatile int16_t  maxtime  = TMAX26;

// Internal
static int16_t  oldlabs_rm = 0;
static int16_t  oldrabs_rm = 0;
static int16_t  oldright   = 0;

// References to sensor_isr.ino variables
extern volatile int16_t   tleft[], tright[];
extern volatile uint8_t   tleftx, trightx;
extern volatile int16_t   tleftmw, trightmw, time0;
extern volatile int16_t   umrabs, umlabs;
extern volatile uint8_t   runflg;
extern volatile uint16_t  tlkurz, trkurz;   // debounce timers, ticked here

// References to B77_TapeCounter_Master.ino
extern volatile float     speedActual;

// References to tape_calc.ino
extern volatile uint8_t   calflg;

// ------------------------------------------------------------
//  runmode_b77_init()
//  Call once from setup() after speedActual is known.
// ------------------------------------------------------------
void runmode_b77_init() {
  dlytime      = DLYTIME_BASE  / NUMSEGS;
  stoptime     = STOPTIME_BASE / NUMSEGS;
  stoptime_init = stoptime;
  midtime = TMID26;
}

// ------------------------------------------------------------
//  runmode_b77()
//  Call every 5 ms from loop() via millis() interval.
//
//  Phase 1 – STOPPED:
//    If runflg == 0 and stoptime > dlytime → declare STOP,
//    zero out ring buffers, clear sync.
//
//  Phase 2 – RUNNING:
//    Compute moving averages tleftmw, trightmw, time0.
//    Detect reel size from time0.
//    Update sync counter.
// ------------------------------------------------------------
void runmode_b77() {

  if (!runflg) {
    // Band not moving (no pulse since last call)
    if (stoptime > dlytime) {
      // Declare STOP: zero ring buffers, clear state
      for (uint8_t i = 1; i <= NUMSEGS; i++) {
        tleft[i]  = 0;
        tright[i] = 0;
      }
      sync      = 0;
      calflg    = 0;
      stopflg   = 255;
      disptime  = 0;
    }
    return;
  }

  // Band is moving
  stopflg   = 0;
  stoptime  = 0;
  runflg    = 0;

  // --- Compute moving averages ---------------------------------
  //  Sum NUMSEGS completed periods from ring buffer
  int16_t tmpL = 0, tmpR = 0;
  uint8_t ix;

  ix = tleftx;
  for (uint8_t i = 0; i < NUMSEGS; i++) {
    if (ix > NUMSEGS) ix = 1;
    tmpL += tleft[ix++];
  }

  ix = trightx;
  for (uint8_t i = 0; i < NUMSEGS; i++) {
    if (ix > NUMSEGS) ix = 1;
    tmpR += tright[ix++];
  }

  tleftmw  = tmpL;
  trightmw = tmpR;
  time0    = (tmpL + tmpR) / 2;   // /2 because double-counted

  // Guard: suppress sudden trightmw drop (EZS sync reset check)
  if (sync && trightmw < oldright - 5) {
    sync = 0;
    disptime = 0;
  }
  oldright = trightmw;

  // --- Reel size detection from time0 --------------------------
  //  Thresholds from config.h, valid once disptime >= dlytime
  if (disptime >= dlytime && time0 > 0) {
    if (time0 > TMIN26) {
      spule   = 26;
      midtime = TMID26;
      mintime = TMIN26;
      maxtime = TMAX26;
    } else if (time0 > TMIN22) {
      spule   = 22;
      midtime = TMID22;
      mintime = TMIN22;
      maxtime = TMAX22;
    } else {
      spule   = 18;
      midtime = TMID18;
      mintime = TMIN18;
      maxtime = TMAX18;
    }
  }

  // --- Sync counter --------------------------------------------
  //  Increment once per full revolution on both reels
  if (abs(umlabs - oldlabs_rm) > NUMSEGS &&
      abs(umrabs - oldrabs_rm) > NUMSEGS) {
    if (sync < 255) sync++;
    oldlabs_rm = umlabs;
    oldrabs_rm = umrabs;
  }
}

// ------------------------------------------------------------
//  runmode_b77_tick()
//  Called every 1 ms from the Timer1 ISR.
//  Increments all timing counters while band is running.
// ------------------------------------------------------------
void runmode_b77_tick() {
  if (!stopflg) {
    // Band running: tick accumulators and ring buffer entry [0]
    if (stoptime < 32000) stoptime++;
    disptime++;
    tleft[0]++;
    tright[0]++;
    tlkurz++;   // debounce counter (declared in sensor_isr.ino)
    trkurz++;
  }
}
