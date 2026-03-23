#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  B77 Master Controller — Configuration
//  Hardware: Arduino Nano Every (ATmega4809)
//  Marc Stähli / HiFiLabor.ch
//  Based on B77_TapeCounter v2.0 + EZS_UNO v1.5
// ============================================================

// ------------------------------------------------------------
//  Software version
// ------------------------------------------------------------
#define SW_VERSION        2.1

// ------------------------------------------------------------
//  Display settings
// ------------------------------------------------------------
#define HELLO_LOGO        true
#define HELLO_TIMEOUT     3000
#define BRIGHTNESS        128
#define FLIPDISPLAY       true
#define INVERTDISPLAY     false
#define OLED_X_OFFSET     2
#define OLED_Y_OFFSET     0
#define I2C_ADDRESS       0x3C

// ------------------------------------------------------------
//  Counter / display format
// ------------------------------------------------------------
#define UNSIGN            false
#define DIGITS_COUNTER    5       // 5-digit counter display
#define DIGITS_MINUTES    3       // 3-digit minutes (000'ss)
#define PULSES_COUNTER    2       // right-reel pulses per counter increment

// ------------------------------------------------------------
//  Tape speed constants  [mm/s]
//  Only 19 cm/s (7.5 ips) and 9.5 cm/s (3.75 ips) supported
// ------------------------------------------------------------
#define SPEEDMIDDLE       190.5f  // 7.5 ips
#define SPEEDLOW           95.25f // 3.75 ips

// ------------------------------------------------------------
//  Reel sensor geometry
//  NUMSEGS: number of white segments on each reel's sensor strip
//  SCOPE:   empirical trim value for counterMeters / counterSeconds
// ------------------------------------------------------------
#define NUMSEGS           4
#define SCOPE             580

// ------------------------------------------------------------
//  Tape reel size thresholds
//  time0 is the mean of tleftmw and trightmw in [ms per revolution]
//  measured at band start when both reels are near their extreme radii.
//  Thresholds adapted from EZS; valid at 19 cm/s with 26 cm spools.
//  mintime / maxtime are used as detection limits for each reel size.
// ------------------------------------------------------------
#define TMIN26            1900
#define TMAX26            4200
#define TMID26            3248
#define TMIN22            1720
#define TMAX22            3450
#define TMID22            2692
#define TMIN18            1050
#define TMAX18            2698
#define TMID18            2044

// Number of right-reel revolutions for tape-thickness calibration
#define UMMAX             30

// ------------------------------------------------------------
//  Stop detection timing  [ms]
//  DLYTIME_BASE / NUMSEGS = period threshold before declaring STOP
//  STOPTIME_BASE / NUMSEGS = initial stoptime counter value
// ------------------------------------------------------------
#define DLYTIME_BASE      8300
#define STOPTIME_BASE     9900

// ------------------------------------------------------------
//  Pin assignments  (Nano Every — all pins 5V tolerant)
//
//  Sensor wiring — Option A (one sensor per reel):
//    SENSOR_A  → RIGHT reel (supply reel at band start)
//                Two white segments on right spool motor
//    SENSOR_B  → LEFT  reel (takeup reel at band start)
//                Two white segments on left spool motor
//    DIRECTION_PIN → B77 REW-signal output
//                    LOW  = rewind active
//                    HIGH = forward (PLAY / FFWD) or idle
//                    Connect via 10k voltage divider if B77
//                    signal is 24V (same as EZS rewpin on A2)
// ------------------------------------------------------------
#define SENSOR_A          3     // INT — right reel primary pulses
#define SENSOR_B          2     // INT — left  reel primary pulses
#define DIRECTION_PIN     4     // INPUT_PULLUP — B77 REW signal
#define BUTTON            5     // Reset / confirm button
#define ROTARY_A          6     // Rotary encoder A
#define ROTARY_B          7     // Rotary encoder B
#define SPEED_PIN         8     // HIGH = 7.5 ips, LOW = 3.75 ips
#define LED               13    // Onboard LED
#define PAUSE_PIN         9     // LOW when machine in PAUSE
#define REW_PIN           10    // Relay output: trigger REWIND
#define STOP_PIN          11    // Relay output: trigger STOP

// ------------------------------------------------------------
//  Display modes  (Encoder rotation cycles through 0..MODE_MAX)
// ------------------------------------------------------------
#define MODE_COUNTER      0     // raw right-reel pulse counter
#define MODE_ELAPSED      1     // elapsed time (counterSeconds)
#define MODE_REMAINING    2     // remaining tape time (resttime)
#define MODE_MAX          2

// ------------------------------------------------------------
//  EEPROM layout  (Nano Every: 256 bytes total)
//  Addresses are chosen to avoid overlap and leave expansion room.
//  Total used: 38 bytes out of 256.
// ------------------------------------------------------------
#define EE_INIT_A         254   // virgin-marker byte A (0xFF = unwritten)
#define EE_INIT_B         255   // virgin-marker byte B

#define EE_MODE             0   // uint8_t   1 byte
#define EE_BRIGHTNESS       1   // uint8_t   1 byte
#define EE_COUNTER          2   // int32_t   4 bytes  [2..5]
#define EE_SECONDS         10   // float     4 bytes  [10..13]
#define EE_OPHOURS         20   // uint32_t  4 bytes  [20..23]
#define EE_MINRIGHTL       30   // int32_t   4 bytes  [30..33]  — calibration
#define EE_LEND            34   // int16_t   2 bytes  [34..35]  — tape thickness µm
#define EE_CC              36   // int16_t   2 bytes  [36..37]  — drift correction *1000

// Offset added to operatingHoursCounter on display (compensate existing hours)
#define OPHOURSOFFSET       0

// ------------------------------------------------------------
//  Rewind-to-zero timing  [seconds]
//
//  COARSE_SECS: switch from full-speed rewind to fine rewind
//               when timeword drops below this value.
//               At 19 cm/s with 26 cm reels ~120 s is safe.
//  FINE_SECS:   trigger final stop when timeword < this value.
//               Corresponds to lockst in EZS (default 40 s).
//  FINAL_SECS:  absolute safety stop regardless of phase.
//  MIN_SECS:    ignore double-click if already closer than this.
//  TIMEOUT_SECS: abort if band makes no progress for this long
//               (jam / broken tape protection).
// ------------------------------------------------------------
#define REW2ZERO_COARSE_SECS   120
#define REW2ZERO_FINE_SECS      30
#define REW2ZERO_FINAL_SECS      5
#define REW2ZERO_MIN_SECS       10
#define REW2ZERO_TIMEOUT_SECS   30

// ------------------------------------------------------------
//  Time helper macros
// ------------------------------------------------------------
#define SECS_PER_MIN      (60UL)
#define SECS_PER_HOUR     (3600UL)
#define SECS_PER_DAY      (SECS_PER_HOUR * 24L)

#define numberOfSeconds(_t_)     ((_t_) % SECS_PER_MIN)
#define numberOfMinutes(_t_)     (((_t_) / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfMinutesEx(_t_)   ((_t_) / SECS_PER_MIN)
#define numberOfHours(_t_)       (((_t_) % SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_t_)         ((_t_) / SECS_PER_DAY)

#endif // CONFIG_H
