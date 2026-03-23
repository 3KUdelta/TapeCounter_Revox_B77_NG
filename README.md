# B77 Master Controller
**Version 2.1 · HiFiLabor.ch · Marc Stähli**

> ⚠️ **Work in progress! Use with caution!**

---

## Overview

The B77 Master Controller is an Arduino-based tape counter and remaining-time display system for the Revox B77 reel-to-reel tape machine. It combines:

- The original **B77 TapeCounter v2.0** (DIYLab / Marc Stähli) with OLED display, rotary encoder navigation and Rewind-to-Zero
- The **remaining-time calculation logic from EZS_UNO v1.5** — tape length calibration and Archimedes spiral formula for accurate remaining time

Target hardware: **Arduino Nano Every** (ATmega4809)

---

## Hardware

### Microcontroller comparison

| Property | Nano Every (ATmega4809) | Nano (ATmega328P) |
|---|---|---|
| SRAM | **6 KB** | 2 KB |
| Flash | **48 KB** | 32 KB |
| EEPROM | 256 bytes | 1024 bytes |
| Timer API | millis() / attachInterrupt() | direct registers |
| Interrupt pins | all digital pins | pin 2 / 3 only |

The Nano Every is required due to the larger SRAM. The float arithmetic for tape length calculation uses approximately 130 bytes of additional variables — critically tight on the 328P.

---

### Pin assignment

```
Arduino Nano Every
┌──────────────────────────────────────────┐
│  Pin  │ Function          │ Direction    │
├───────┼───────────────────┼──────────────┤
│   2   │ SENSOR_B          │ INPUT        │  left  reel — sensor pulses
│   3   │ SENSOR_A          │ INPUT        │  right reel — sensor pulses
│   4   │ DIRECTION_PIN     │ INPUT_PU     │  B77 REW signal (LOW = rewind)
│   5   │ BUTTON            │ INPUT_PU     │  reset / confirm button
│   6   │ ROTARY_A          │ INPUT        │  rotary encoder channel A
│   7   │ ROTARY_B          │ INPUT        │  rotary encoder channel B
│   8   │ SPEED_PIN         │ INPUT        │  speed sense (HIGH=7.5ips)
│   9   │ PAUSE_PIN         │ INPUT_PU     │  B77 pause signal (LOW=pause)
│  10   │ REW_PIN           │ OUTPUT       │  relay: trigger rewind
│  11   │ STOP_PIN          │ OUTPUT       │  relay: trigger stop
│  13   │ LED               │ OUTPUT       │  onboard LED (save indicator)
│  A4   │ SDA               │ I²C          │  OLED display
│  A5   │ SCL               │ I²C          │  OLED display
└──────────────────────────────────────────┘
INPUT_PU = INPUT_PULLUP
```

---

### Sensor configuration (Option A)

**Important:** Each reel has its own dedicated sensor. This differs from the original B77 TapeCounter (which used two sensors on the right reel for quadrature direction detection).

```
Left  reel ←──── SENSOR_B (HW-006 module, pin 2)
                 4 white segments on left spool axle

Right reel ←──── SENSOR_A (HW-006 module, pin 3)
                 4 white segments on right spool axle

Direction  ←──── DIRECTION_PIN (pin 4)
                 connected to B77 REW signal output
```

**Reel physics during PLAY (forward):**

```
Tape travels from left to right:
  Left  reel: pays out tape → radius shrinks → revolutions speed up
  Right reel: takes up tape → radius grows   → revolutions slow down
```

The **remaining time** is calculated from the left reel radius (`tleftmw`). Without a dedicated sensor on the left reel, accurate remaining-time calculation is not possible.

---

### SENSOR_B relocation (right → left reel)

The original B77 TapeCounter placed both HW-006 modules on the **right** reel. For version 2.1, one module must be relocated:

1. Remove one HW-006 module from the right reel axle
2. Attach a white segment strip (4 segments × 90°) to the **left** reel axle
3. Mount the HW-006 so the IR gate cleanly reads the segments
4. Re-route the cable to pin 2 on the Nano

---

### DIRECTION_PIN — connecting to the B77

The B77 provides a REW signal on its remote-control connector that is LOW while rewind is active.

**Voltage divider (24V → 5V) — recommended transistor circuit:**

```
B77 REW output (24V) ──[10 kΩ]──┬── Base of BC338 (or similar NPN)
                                  │
                                 GND
Collector BC338 ─────────────────── Arduino pin 4 (INPUT_PULLUP)
Emitter   BC338 ─────────────────── GND

Logic:
  REW active   (24V) → transistor conducts → pin 4 = LOW  ✓
  REW inactive  (0V) → transistor off      → pin 4 = HIGH (pull-up) ✓
```

This is identical to the `rewpin` circuit in the EZS_UNO schematic.

---

### Speed detection (SPEED_PIN)

```
B77 collector Q1 — Capstan Speed Control PCB
  12V at 7.5 ips (19 cm/s)
   0V at 3.75 ips (9.5 cm/s)

Voltage divider: R1 = 10 kΩ, R2 = 6.8 kΩ
  12V × 6.8 / 16.8 = 4.86V → HIGH on Nano Every (5V tolerant) ✓
   0V → LOW ✓

Arduino pin 8:  HIGH = 7.5 ips  (SPEEDMIDDLE = 190.5 mm/s)
                LOW  = 3.75 ips (SPEEDLOW    =  95.25 mm/s)
```

---

### Rewind-to-Zero relays

```
Arduino pin 10 (REW_PIN)  ──→ relay → connects 24V to remote pin 3
Arduino pin 11 (STOP_PIN) ──→ relay → connects 24V to remote pin 6

Pulse: 10 ms HIGH, then LOW again
  The relay fires briefly and triggers the respective command.
```

---

### Pause input

```
B77 Pause PCB (e.g. Mauro200id mod, PIC foot 6)
  LOW  = pause active
  HIGH = normal operation

Arduino pin 9 (PAUSE_PIN, INPUT_PULLUP)
  Note: pin is LOW for ~3 seconds at startup (B77 initialisation)
  → software delay of 3s suppresses false pause detection on boot
```

---

### OLED display

```
SSD1306, 128×32 pixels, I²C
  I²C address: 0x3C  (0x3D on some modules)
  SDA → Arduino A4
  SCL → Arduino A5
  VCC → 3.3V or 5V (depending on module)
  GND → GND

Orientation: 180° rotated (FLIPDISPLAY = true)
```

---

### Rotary encoder

```
EC12 RE1 (360°, with push button)
  Channel A → pin 6
  Channel B → pin 7
  Button    → pin 5 (shared with BUTTON / reset)
  GND       → GND
  VCC       → 3.3V or 5V

Library: Encoder.h (Paul Stoffregen)
  ENCODER_DO_NOT_USE_INTERRUPTS defined
  → polled in loop(), no interrupt conflicts
```

---

## Software architecture

### File structure

```
B77_Master/
├── B77_TapeCounter_Master.ino   Main file: setup(), loop(), display,
│                                 encoder, button, speed detection
├── sensor_isr.ino               ISR for both reel sensors
├── runmode.ino                  5 ms task: moving average calculation,
│                                 STOP detection, reel size detection
├── tape_calc.ino                Calibration and remaining-time calculation
│                                 (ported from EZS_UNO v1.5)
├── config.h                     All constants, pins, EEPROM layout
├── tapecounter_16x24.h          OLED font 16×24 px
├── tapecounter_21x32.h          OLED font 21×32 px
└── logo_mst1.h                  Splash image 128×32 px (RevVox logo)
```

---

### Call sequence in loop()

```
loop() — every iteration (~1 ms)
  │
  ├── btn.tick()                      OneButton polling
  ├── read encoder                    every 4 steps → EncoderRotated()
  ├── check PAUSE_PIN                 display "PAUSE" / restore display
  │
  ├── every 5 ms:
  │     runmode_b77_tick()            tick ms-counters and ring buffer accumulators
  │     runmode_b77()                 moving averages, STOP detection
  │     tape_restlauf()               remaining time (Archimedes, 1×/revolution)
  │     tape_lenrout()                absolute position, drift correction
  │
  ├── every 30 ms:
  │     CalculatingDisplayParameters() poll speed input
  │     RefreshDisplay()               update OLED (if not locked)
  │
  ├── every 500 ms:
  │     update isMoving               used by Rew2Zero logic
  │
  ├── every 1000 ms:
  │     operatingHoursCounter++       operating hours
  │     saveCounter++                 EEPROM save trigger
  │
  └── when saveCounter >= 4:
        SaveCounter()                 write EEPROM + blink LED
```

---

### Interrupt service routines

```
CheckState()   — SENSOR_A CHANGE (right reel, pin 3)
  Falling edge: tlkurz = 0
  Rising edge (if tlkurz >= 1 ms):
    → rotate tright[] ring buffer
    → umrabs ±1
    → counter ±1 (every PULSES_COUNTER = 2 pulses)
    → accumulate counterMeters, counterSeconds
    → runflg++

CheckStateB()  — SENSOR_B CHANGE (left reel, pin 2)
  Falling edge: trkurz = 0
  Rising edge (if trkurz >= 1 ms):
    → rotate tleft[] ring buffer
    → umlabs ±1
    → runflg++

Direction in both ISRs:
  dirForward = (digitalRead(DIRECTION_PIN) == HIGH)
```

---

## Display modes

The rotary encoder cycles through three modes:

| Mode | Display | Description |
|---|---|---|
| 0 | ` 12345` | Pulse counter (right reel, 5 digits, signed) |
| 1 | ` 042'37` | Elapsed time in min'sec (from counterSeconds) |
| 2 | `R045'12` | **Remaining time** in min'sec (from resttime) |

In mode 2 while calibration is running:
```
CAL 24    ← remaining revolutions until calibration completes
```

---

## Operation

### Normal use

| Action | Function |
|---|---|
| Rotate encoder clockwise | Next mode (0→1→2→0) |
| Rotate encoder counter-clockwise | Previous mode (0→2→1→0) |
| Single click | Reset all counters (counter, counterSeconds, resttime, timeword) |
| Double click | Start Rewind-to-Zero (if counter > 0 or timeword > 10 s) |
| Long press | Adjust brightness (16 steps, blinks on save) |
| Hold button at power-on | Display operating hours |

### Rewind-to-Zero

The procedure uses `timeword` (tape position in seconds) as its reference — independent of reel size and tape speed:

```
Phase 0 (coarse):  timeword >= 120 s  → full rewind speed
                   timeword <  120 s  → doStop() → phase 1

Phase 1 (fine):    band stopped → doRewind() (short burst)
                   timeword <   30 s  → doStop() → phase 2

Phase 2 (done):    band stopped → rewindToZero = false, refresh display

Safety:            timeword <=   5 s  → emergency stop (any phase)
                   no movement for 30 s → abort (tape jam / break)
```

---

## Remaining time calibration

### Physics

The remaining time is based on the **Archimedes spiral** of the tape wound on the reel:

```
r₀  = inner radius of right reel at band start
       = minrightl / (2π)

r₁  = current outer radius of left reel
       = (tleftmw × speed) / (2π)

Remaining time = π × (r₁² − r₀²) / (lend × timekorr × speed)

lend     = tape thickness in µm  (from auto-calibration)
timekorr = drift correction factor (starts at 1.0, adapts slowly)
```

### Calibration sequence

Calibration starts automatically at **band start** (right reel reaches minimum period → `trightmw < mintime`):

```
1. Band start detected:
   → minrightl = trightmw × speedActual    (reference inner radius)
   → calflg = 1
   → OLED shows "CAL xx" (countdown)

2. During UMMAX = 30 revolutions:
   → period growth of right reel is measured

3. After 30 revolutions:
   → lend (tape thickness µm) computed from radius growth
   → calflg = 0
   → values saved to EEPROM
   → remaining-time display starts

4. On subsequent playbacks:
   → cc (drift corrector × 1000) adapts slowly
   → range: 900..1100  (±10% correction window)
```

### Typical tape thickness values

| Reel type | Tape thickness | `lend` value |
|---|---|---|
| 26 cm reel, standard tape | ~35 µm | 35 |
| 26 cm reel, long-play tape | ~26 µm | 26 |
| 22 cm reel | ~26–35 µm | depends on tape |

---

## EEPROM layout (Nano Every, 256 bytes)

| Address | Size | Variable | Content |
|---|---|---|---|
| 0 | 1 B | `EE_MODE` | Last display mode (0/1/2) |
| 1 | 1 B | `EE_BRIGHTNESS` | Display brightness (0–16) |
| 2–5 | 4 B | `EE_COUNTER` | Pulse counter (int32) |
| 10–13 | 4 B | `EE_SECONDS` | Elapsed time (float) |
| 20–23 | 4 B | `EE_OPHOURS` | Operating hours (uint32) |
| 30–33 | 4 B | `EE_MINRIGHTL` | Calibration: r₀ metric (int32) |
| 34–35 | 2 B | `EE_LEND` | Tape thickness µm (int16) |
| 36–37 | 2 B | `EE_CC` | Drift correction factor × 1000 (int16) |
| 254 | 1 B | `EE_INIT_A` | First-run initialisation marker |
| 255 | 1 B | `EE_INIT_B` | First-run initialisation marker |
| **total** | **38 B** | | **218 B free** |

EEPROM is written:
- **Every 4 seconds** while the tape is running (counter state, operating hours)
- **Every 10 minutes** operating hours backup
- **After calibration** `lend`, `minrightl`, `cc`
- **On brightness change** and **mode change**

---

## Configurable parameters (`config.h`)

### Display

| Parameter | Default | Description |
|---|---|---|
| `HELLO_LOGO` | `true` | Show splash logo |
| `HELLO_TIMEOUT` | `3000` | Splash display duration [ms] |
| `BRIGHTNESS` | `128` | Initial brightness (0–255) |
| `FLIPDISPLAY` | `true` | Display rotated 180° |
| `INVERTDISPLAY` | `false` | Inverted display |
| `OLED_X_OFFSET` | `2` | Horizontal pixel offset |
| `OLED_Y_OFFSET` | `0` | Vertical pixel offset |
| `I2C_ADDRESS` | `0x3C` | I²C address (0x3C or 0x3D) |
| `DIGITS_COUNTER` | `5` | Counter display digits |
| `DIGITS_MINUTES` | `3` | Minutes digits in time display |

### Sensor geometry

| Parameter | Default | Description |
|---|---|---|
| `NUMSEGS` | `4` | Segments per reel revolution |
| `SCOPE` | `580` | Trim value for meter/seconds display |
| `PULSES_COUNTER` | `2` | Pulses per counter increment |

### Tape speed

| Parameter | Value | Description |
|---|---|---|
| `SPEEDMIDDLE` | `190.5 mm/s` | 7.5 ips (19 cm/s) |
| `SPEEDLOW` | `95.25 mm/s` | 3.75 ips (9.5 cm/s) |

### Reel size detection thresholds [ms/revolution]

| Constant | 26 cm | 22 cm | 18 cm |
|---|---|---|---|
| `TMIN` | 1900 | 1720 | 1050 |
| `TMID` | 3248 | 2692 | 2044 |
| `TMAX` | 4200 | 3450 | 2698 |

Values from EZS_UNO v1.5 (empirically measured at 19 cm/s with Revox/BASF tapes).

### Calibration

| Parameter | Default | Description |
|---|---|---|
| `UMMAX` | `30` | Revolutions for tape-thickness calibration |

### Rewind-to-Zero [seconds]

| Parameter | Default | Description |
|---|---|---|
| `REW2ZERO_COARSE_SECS` | `120` | Switch to fine rewind below this position |
| `REW2ZERO_FINE_SECS` | `30` | Final stop threshold |
| `REW2ZERO_FINAL_SECS` | `5` | Absolute emergency stop |
| `REW2ZERO_MIN_SECS` | `10` | Minimum position to trigger procedure |
| `REW2ZERO_TIMEOUT_SECS` | `30` | Abort if no movement for this long |

---

## Libraries

| Library | Source | Purpose |
|---|---|---|
| `OneButton` | mathertel/OneButton | Single / double / long-press detection |
| `Encoder` | PaulStoffregen/Encoder | Rotary encoder (polled mode) |
| `EEPROM` | Arduino built-in | Persistent data storage |
| `SSD1306Ascii` | greiman/SSD1306Ascii | OLED driver (no U8g2, saves Flash) |
| `Wire` | Arduino built-in | I²C for OLED |

---

## Troubleshooting

### OLED permanently shows `CAL xx`

Calibration has never completed successfully. Possible causes:
- Tape is not at band start (right reel not yet empty enough)
- `TMIN26` threshold too high → `trightmw < mintime` never triggered
- Sensor on right reel faulty or misaligned

Fix: Fully rewind the tape, then press PLAY and allow 30 revolutions to complete.

### Remaining time jumps or is inaccurate

- `lend` calibrated incorrectly → trigger a fresh calibration (fully rewind, then play)
- `SCOPE` too far from 580 → `counterSeconds` drifts from real time → `cc` cannot converge
- Sensor segments dirty → irregular pulses → ring buffer receives outliers

### Rewind-to-Zero stops too early or too late

Adjust values in `config.h`:
- Stops too early: reduce `REW2ZERO_FINE_SECS`
- Stops too late: reduce `REW2ZERO_COARSE_SECS`
- At 3.75 ips longer braking distances: increase `REW2ZERO_COARSE_SECS` to 180

### DIRECTION_PIN reads wrong direction

- Check voltage divider: pin 4 must be < 0.8V when REW is active
- Use the transistor circuit (more reliable level shifting)
- Measure with a multimeter: B77 REW output = 24V active, 0V inactive

---

## Changelog

| Version | Date | Changes |
|---|---|---|
| 1.0 | 2020 | Original DIYLab B77 TapeCounter |
| 2.0 | Oct 2023 | Marc Stähli: B77 MKI adaptations, encoder, pause, Rewind-to-Zero |
| 2.1 | Mar 2026 | Remaining-time calculation (EZS port), Nano Every target, Option-A dual-reel sensors, timeword-based Rewind-to-Zero, new `sensor_isr.ino` / `runmode.ino` / `tape_calc.ino` |

---

## Credits & references

- EZS_UNO v1.5 — tape length and remaining-time logic (Archimedes spiral)
- [github.com/3KUdelta/TapeCounter_Revox_B77](https://github.com/3KUdelta/TapeCounter_Revox_B77) — original project v2.0
- [old-fidelity-forum.de](https://old-fidelity-forum.de/thread-38940.html) — community contributions, user "gogosch" (speed formula)
- Revox B77 Service Manual — remote connector pinout, transistor Q1 capstan PCB
