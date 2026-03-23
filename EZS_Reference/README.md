# EZS_UNO v1.5 — Reference Sources

These are the original source files from the **EZS_UNO v1.5** project,
which provided the tape-length calibration and remaining-time calculation
logic (Archimedes spiral formula) ported into the B77 Master Controller v2.1.

| File | Description |
|---|---|
| `EZS_UNO_15.ino` | Main file — setup, loop, runmode() |
| `auswert.ino` | IR input evaluation, transport commands |
| `eeprom.ino` | EEPROM read/write routines |
| `irlearn.ino` | IR remote learning mode |
| `lcd.ino` | LCD helper functions |
| `subroutines.ino` | restlauf(), lenrout(), menus — core physics ported to tape_calc.ino |
| `main.h` | Pin definitions, constants, EEPROM macros |
| `Readme_1_5.txt` | Original EZS v1.5 changelog (German) |
| `EZS_A77_Schem.pdf` | Circuit schematic for A77/B77 hardware interface |
| `B77_TapeCounter.ino` | Original B77 TapeCounter v2.0 (pre-integration) |

These files are included for reference only and are not compiled
as part of the B77 Master Controller project.
