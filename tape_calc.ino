// ============================================================
//  tape_calc.ino
//  Remaining tape time calculation
//
//  Port of EZS subroutines:
//    restlauf()  →  tape_restlauf()
//    lenrout()   →  tape_lenrout()
//
//  Simplifications vs EZS original:
//    - No reverse play mode
//    - No TEAC / Uher / A77 variants
//    - Only 19 cm/s and 9.5 cm/s (SPEEDMIDDLE / SPEEDLOW)
//    - float instead of double (sufficient precision on 4809,
//      and faster than double on AVR-family)
//    - bandd = 0 always triggers auto-calibration (no manual entry)
//
//  Physics (Archimedes spiral of tape wound on reel):
//    r0  = minrightl / (2π)              — inner radius at band start
//    r1  = (trightmw * speed) / (2π)     — current outer radius
//    resttime = π * (r1² - r0²) / (lend * timekorr * speed)
//
//  lend: tape thickness in µm (determined by calibration over UMMAX revs)
//  timekorr: drift correction factor, starts at 1.0, adapts slowly
//  cc: integer form of timekorr * 1000 (range 900..1100, default 1000)
// ============================================================

#include "config.h"
#include <math.h>

// ------------------------------------------------------------
//  Exported state
// ------------------------------------------------------------
volatile uint8_t  calflg    = 0;    // 1 = calibration in progress
volatile int16_t  resttime  = 0;    // remaining tape time in seconds
volatile int16_t  abstime   = 0;    // elapsed time in seconds (calculated)
volatile int16_t  timeword  = 0;    // running tape position in seconds

// Calibration results (saved to EEPROM after first full calibration)
int32_t  minrightl = 0;             // inner radius metric: trightmw * speed at band start
int16_t  lend      = 35;            // tape thickness in µm (result of calibration)
int16_t  cc        = 1000;          // correction factor * 1000

// ------------------------------------------------------------
//  Internal state
// ------------------------------------------------------------
static float   len0q     = 0.0f;   // r0² (from calibration)
static float   lenword   = 0.0f;   // total tape length from start [mm]
static float   lastlen   = 0.0f;   // previous lenword for speed calc
static float   timekorr  = 1.0f;   // = cc / 1000.0
static float   lendiff   = 0.0f;   // delta length per revolution [µm]
static int16_t lasttim   = 0;
static int16_t urold     = 0;      // previous umrabs/rdivisor value
static int16_t ukold     = 0;
static int16_t lastu16   = 0;
static int16_t u16l      = 0;      // previous umabs value
static bool    umflg     = true;   // reset guard

#define RDIVISOR  NUMSEGS           // = 4 pulses per revolution
#define LDIVISOR  NUMSEGS

// Pi as float
static const float PI_F = 3.14159265f;

// References from other modules
extern volatile int16_t  tleftmw, trightmw, time0;
extern volatile int16_t  umrabs, umlabs;
extern volatile uint8_t  sync, stopflg;
extern volatile uint8_t  spule;
extern volatile int16_t  mintime;
extern volatile float    speedActual;

// ------------------------------------------------------------
//  tape_calc_load()
//  Load calibration data from EEPROM.
//  Call once from setup() after EEPROM init.
// ------------------------------------------------------------
#include <EEPROM.h>
void tape_calc_load() {
  EEPROM.get(EE_MINRIGHTL, minrightl);
  EEPROM.get(EE_LEND,      lend);
  EEPROM.get(EE_CC,        cc);

  // Sanity checks
  if (minrightl <= 0)          minrightl = 0;
  if (lend < 10 || lend > 60)  lend = 35;    // 35 µm = typical 26 cm reel
  if (cc   < 900 || cc > 1100) cc   = 1000;

  timekorr = (float)cc / 1000.0f;
  len0q    = (float)minrightl;
  len0q   /= (PI_F * 2.0f);
  len0q   *= len0q;
}

// ------------------------------------------------------------
//  tape_calc_save()
//  Save calibration results to EEPROM.
//  Called after calflg transitions 1→0.
// ------------------------------------------------------------
void tape_calc_save() {
  EEPROM.put(EE_MINRIGHTL, minrightl);
  EEPROM.put(EE_LEND,      lend);
  EEPROM.put(EE_CC,        cc);
}

// ------------------------------------------------------------
//  tape_restlauf()
//  Port of EZS restlauf() — remaining time calculation.
//
//  Must be called from the main loop (not ISR).
//  Frequency: every loop iteration, but internal guards
//  ensure heavy float work runs only once per revolution.
//
//  State machine:
//    sync < SYNCKST  →  spooling / startup: update resttime
//                        from timeword delta only (no float calc)
//    trightmw < mintime + guard  →  band start detected:
//                        arm calibration (calflg = 1)
//    calflg = 1      →  accumulate UMMAX revolutions,
//                        compute lend, transition to normal
//    normal          →  full Archimedes spiral calculation
//                        once per revolution
// ------------------------------------------------------------
void tape_restlauf() {

  if (sync < SYNCKST) {
    // ---- Startup / spooling phase ----------------------------
    // Track resttime by measuring timeword delta
    if (!lasttim) lasttim = timeword;   // oneshot init
    resttime += lasttim - timeword;     // direction-aware delta
    lasttim   = timeword;
    return;
  }

  // In sync — full calculation path
  lasttim = 0;

  // ---- Band start detection (reset point) --------------------
  // trightmw < mintime means right reel is near minimum size
  // → we are at band start, arm calibration
  if (umflg && trightmw < mintime) {
    umflg     = false;
    timeword  = 0;
    umlabs    = 0;
    umrabs    = 0;
    // Arm calibration
    urold     = 0;
    calflg    = 1;
    resttime  = 0;
    minrightl = (int32_t)trightmw * (int16_t)(speedActual);
    len0q     = (float)minrightl;
    len0q    /= (PI_F * 2.0f);
    len0q    *= len0q;
    cc        = 1000;
    timekorr  = 1.0f;
    lend      = 35;    // fallback; will be refined during UMMAX revs
  }

  if (trightmw > mintime) umflg = true;  // reset guard

  // ---- Calibration phase -------------------------------------
  if (calflg) {
    int16_t current_rev = umrabs / RDIVISOR;
    if (current_rev != urold) {
      urold = current_rev;
      if (umrabs >= (int16_t)(UMMAX * RDIVISOR)) {
        // UMMAX revolutions completed — compute tape thickness
        float tmpdbl = (float)trightmw * speedActual
                       - (float)minrightl;
        tmpdbl /= (PI_F * 2.0f);
        tmpdbl += (float)(umrabs / RDIVISOR) / 2.0f;
        tmpdbl /= (float)(umrabs / RDIVISOR);
        lend    = (int16_t)tmpdbl;
        calflg  = 0;
        tape_calc_save();
      }
    }
    return;   // no resttime calc during calibration
  }

  // ---- Normal operation: resttime from Archimedes spiral -----
  // Execute once per full revolution of right reel
  int16_t current_rev = umrabs / RDIVISOR;
  if (current_rev == urold) return;   // no new revolution yet
  urold = current_rev;

  timekorr = (float)cc / 1000.0f;

  float len1q = (float)tleftmw * speedActual;  // left reel outer radius
  len1q /= (PI_F * 2.0f);
  len1q *= len1q;

  float tmpdbl = PI_F * (len1q - len0q)
                 / ((float)lend * timekorr);   // remaining length in µm
  tmpdbl /= speedActual;
  resttime = (int16_t)(tmpdbl / 1000.0f);      // µm/mm → seconds
}

// ------------------------------------------------------------
//  tape_lenrout()
//  Port of EZS lenrout() — absolute position calculation.
//
//  Computes abstime (seconds from band start) and timeword
//  (running counter, synchronised to counterSeconds in play mode).
//  Also updates cc (drift correction) once per right-reel revolution.
//
//  Only runs when umrabs has changed since last call.
// ------------------------------------------------------------
void tape_lenrout() {

  int16_t umabs_now = (int16_t)abs(umrabs);

  if (umabs_now == u16l) return;    // no change

  u16l      = umabs_now;
  timekorr  = (float)cc / 1000.0f;

  // Total tape length from band start [mm]
  float tmpdbl = (float)lend * PI_F * timekorr;
  tmpdbl *= (float)u16l;
  tmpdbl /= (float)RDIVISOR;
  tmpdbl += (float)minrightl;
  tmpdbl *= (float)u16l;
  tmpdbl /= (float)RDIVISOR;
  lenword = tmpdbl / 1000.0f;       // µm → mm

  // Delta length for speed display (one revolution worth)
  lendiff  = fabsf(lastlen - tmpdbl);
  lendiff *= (float)RDIVISOR;
  int16_t udiff = abs(u16l - lastu16);
  if (udiff > 0) {
    lendiff += (float)udiff / 2.0f;
    lendiff /= (float)udiff;
  }
  lastu16 = u16l;
  lastlen = tmpdbl;

  // Elapsed time in seconds
  abstime = (int16_t)((lenword + speedActual / 2.0f) / speedActual);

  // timeword: follow abstime when not in steady play
  // In steady play (sync >= SYNCKST): counterSeconds tracks it instead
  if (sync < SYNCKST) {
    timeword = abstime;
  } else {
    // Once per right-reel revolution: adjust drift correction cc
    int16_t rev_now = umrabs / RDIVISOR;
    if (rev_now != ukold) {
      ukold = rev_now;
      if (abstime + 1 < timeword) {
        cc++;
        if (cc > 1100) cc = 1100;
      } else if (abstime - 1 > timeword) {
        cc--;
        if (cc < 900) cc = 900;
      }
    }
  }
}

// ------------------------------------------------------------
//  tape_calc_getRemainingStr()
//  Format resttime as "REM -mm'ss" for OLED display.
//  buf must be at least 10 chars.
// ------------------------------------------------------------
void tape_calc_getRemainingStr(char* buf) {
  if (calflg) {
    // Show calibration progress
    int16_t remaining_revs = (int16_t)(UMMAX)
                             - (umrabs / RDIVISOR);
    if (remaining_revs < 0) remaining_revs = 0;
    snprintf(buf, 10, "CAL %2d", (int)remaining_revs);
  } else if (resttime <= 0) {
    snprintf(buf, 10, " ---'--");
  } else {
    int16_t t = resttime;
    int mins  = numberOfMinutesEx(t);
    int secs  = numberOfSeconds(t);
    snprintf(buf, 10, "R%03d'%02d", mins, secs);
  }
}
