# B77 Master Controller — Dokumentation
**Version 2.1 · HiFiLabor.ch · Marc Stähli**

---

## Übersicht

Der B77 Master Controller ist ein Arduino-basiertes Zähler- und Restzeitanzeigesystem für die Revox B77 Tonbandmaschine. Er kombiniert:

- Den originalen **B77 TapeCounter v2.0** (DIYLab / Marc Stähli) mit OLED-Anzeige, Rotary-Encoder-Bedienung und Rewind-to-Zero
- Die **Restzeit-Berechnungslogik aus EZS_UNO v1.5** (Bandlängenkalibrierung und Archimedes-Spiralformel)

Zielplattform: **Arduino Nano Every** (ATmega4809)

---

## Hardware

### Mikrocontroller

| Eigenschaft | Nano Every (ATmega4809) | Nano (ATmega328P) |
|---|---|---|
| SRAM | **6 KB** | 2 KB |
| Flash | **48 KB** | 32 KB |
| EEPROM | 256 Bytes | 1024 Bytes |
| Timer-API | millis() / attachInterrupt() | direkte Register |
| Interrupt-Pins | alle digitalen Pins | nur pin 2 / 3 |

Der Nano Every wird wegen des grösseren SRAM verwendet. Die Float-Arithmetik der Bandlängenberechnung belegt ca. 130 Bytes zusätzliche Variablen — auf dem 328P war das kritisch knapp.

---

### Anschlussplan (Pinbelegung)

```
Arduino Nano Every
┌─────────────────────────────────────┐
│  Pin  │ Funktion          │ Richtung │
├───────┼───────────────────┼──────────┤
│   2   │ SENSOR_B          │ INPUT    │  linke  Spule — Sensor-Pulse
│   3   │ SENSOR_A          │ INPUT    │  rechte Spule — Sensor-Pulse
│   4   │ DIRECTION_PIN     │ INPUT_PU │  B77 REW-Signal (LOW=Rückspulen)
│   5   │ BUTTON            │ INPUT_PU │  Reset-/Bestätigungstaste
│   6   │ ROTARY_A          │ INPUT    │  Rotary-Encoder Kanal A
│   7   │ ROTARY_B          │ INPUT    │  Rotary-Encoder Kanal B
│   8   │ SPEED_PIN         │ INPUT    │  Geschwindigkeitssignal
│   9   │ PAUSE_PIN         │ INPUT_PU │  B77 Pause-Signal (LOW=Pause)
│  10   │ REW_PIN           │ OUTPUT   │  Relais: Rückspulen auslösen
│  11   │ STOP_PIN          │ OUTPUT   │  Relais: Stop auslösen
│  13   │ LED               │ OUTPUT   │  Onboard-LED (Speicher-Indikator)
│  A4   │ SDA               │ I²C      │  OLED Display
│  A5   │ SCL               │ I²C      │  OLED Display
└─────────────────────────────────────┘
INPUT_PU = INPUT_PULLUP
```

---

### Sensor-Konfiguration (Option A)

**Wichtig:** Jede Spule hat einen eigenen Sensor. Dies ist abweichend vom originalen B77 TapeCounter (der zwei Sensoren auf der rechten Spule für Quadratur-Richtungserkennung verwendet).

```
Linke Spule  ←──── SENSOR_B (HW-006, pin 2)
                   4 weisse Segmente auf Spulenachse

Rechte Spule ←──── SENSOR_A (HW-006, pin 3)
                   4 weisse Segmente auf Spulenachse

Richtung     ←──── DIRECTION_PIN (pin 4)
                   vom B77 REW-Signal-Ausgang
```

**Physik der Spulenfunktion bei PLAY (Vorwärtslauf):**

```
Band läuft von links nach rechts:
  Linke Spule:   gibt Band ab → Radius schrumpft → Umdrehungen schneller
  Rechte Spule:  nimmt Band auf → Radius wächst  → Umdrehungen langsamer
```

Die **Restzeit** wird aus dem Radius der linken Spule berechnet (`tleftmw`). Ohne Sensor auf der linken Spule wäre keine korrekte Restzeit-Berechnung möglich.

---

### SENSOR_B Umbau (von rechts auf links)

Der originale B77 TapeCounter hatte beide HW-006-Module auf der **rechten** Spule. Für Version 2.1 muss einer umgebaut werden:

1. HW-006 von der rechten Spulenachse demontieren
2. Weissen Segmentstreifen (4 Segmente à 90°) auf der **linken** Spulenachse anbringen
3. HW-006 so montieren, dass die IR-Lichtschranke die Segmente sauber erfasst
4. Kabelweg neu verlegen zu Pin 2 am Nano

---

### DIRECTION_PIN — Anschluss am B77

Der B77 stellt an seinen Fernsteuer-Pins ein REW-Signal bereit, das LOW ist, solange das Rückspulen aktiv ist.

**Spannungsanpassung (24V → 5V):**

```
B77 REW-Ausgang (24V)
        │
       [10 kΩ]
        │
        ├──────────── Arduino Pin 4
        │
       [6.8 kΩ]
        │
       GND

Spannung an Pin 4:  24V × 6.8 / (10 + 6.8) = 9.7V  → zu hoch!

Besser: 10 kΩ / 3.3 kΩ
        24V × 3.3 / 13.3 = 5.9V  → noch etwas zu hoch für 5V-Eingang

Empfehlung: 10 kΩ / 2.7 kΩ
        24V × 2.7 / 12.7 = 5.1V  → grenzwertig

Sicherer: Zener 5.1V parallel zu unterem Widerstand
  oder: kleiner NPN-Transistor als Pegelwandler (wie im EZS-Schaltbild)
```

**Transistor-Variante (wie EZS):**

```
B77 REW (24V) ──[10kΩ]──┬── Basis BC338
                         │
                        GND
Kollektor BC338 ─────────── Arduino Pin 4 (INPUT_PULLUP)
Emitter  BC338 ──────────── GND

Logik: REW aktiv (24V) → Transistor schaltet durch → Pin 4 = LOW  ✓
       REW inaktiv (0V) → Transistor sperrt → Pin 4 = HIGH (Pullup) ✓
```

---

### Geschwindigkeitserkennung (SPEED_PIN)

```
B77 Kollektor Q1 (Capstan Speed Control PCP)
  12V bei 7.5 ips (19 cm/s)
   0V bei 3.75 ips (9.5 cm/s)

Spannungsteiler: R1 = 10 kΩ, R2 = 6.8 kΩ
  12V × 6.8 / 16.8 = 4.86V → HIGH am Nano Every (5V tolerant) ✓
   0V → LOW ✓

Arduino Pin 8:  HIGH = 7.5 ips (SPEEDMIDDLE = 190.5 mm/s)
                LOW  = 3.75 ips (SPEEDLOW    =  95.25 mm/s)
```

---

### Rewind-to-Zero Relais

```
Arduino Pin 10 (REW_PIN)  ──→ Relais → verbindet 24V Fernsteuer-Pin 3
Arduino Pin 11 (STOP_PIN) ──→ Relais → verbindet 24V Fernsteuer-Pin 6

Puls: 10 ms HIGH, dann wieder LOW
  Relais schaltet kurz und löst den jeweiligen Befehl aus.
```

---

### Pause-Eingang

```
B77 Pause-PCB (z.B. Mauro200id-Umbau, PIC Foot 6)
  LOW  wenn Pause aktiv
  HIGH wenn normal

Arduino Pin 9 (PAUSE_PIN, INPUT_PULLUP)
  Beim Einschalten: Pin ist ca. 3 Sekunden LOW (Initialisierung B77)
  → Software-Delay von 3s beim ersten Erkennen unterdrückt Fehlauslösung
```

---

### OLED Display

```
SSD1306, 128×32 Pixel, I²C
  I²C-Adresse: 0x3C  (je nach Modul auch 0x3D)
  SDA → Arduino A4
  SCL → Arduino A5
  VCC → 3.3V oder 5V (je nach Modul)
  GND → GND

Ausrichtung: 180° gedreht (FLIPDISPLAY = true)
```

---

### Rotary Encoder

```
EC12 RE1 (360°, mit Drucktaste)
  Kanal A → Pin 6
  Kanal B → Pin 7
  Taste   → Pin 5 (BUTTON, gemeinsam mit Reset)
  GND     → GND
  VCC     → 3.3V oder 5V

Bibliothek: Encoder.h (Paul Stoffregen)
  ENCODER_DO_NOT_USE_INTERRUPTS gesetzt
  → polling im Loop, keine Interrupt-Konflikte
```

---

## Software-Architektur

### Dateistruktur

```
B77_Master/
├── B77_TapeCounter_Master.ino   Hauptdatei: setup(), loop(), Display,
│                                 Encoder, Button, Geschwindigkeit
├── sensor_isr.ino               ISR für beide Spulensensoren
├── runmode.ino                  5-ms-Task: gleitender Mittelwert,
│                                 STOP-Erkennung, Spulengrösse
├── tape_calc.ino                Kalibrierung und Restzeit-Berechnung
│                                 (portiert aus EZS_UNO v1.5)
├── config.h                     Alle Konstanten, Pins, EEPROM-Layout
├── tapecounter_16x24.h          OLED-Font 16×24 px
├── tapecounter_21x32.h          OLED-Font 21×32 px
└── logo_mst1.h                  Startbild 128×32 px (RevVox Logo)
```

---

### Aufrufreihenfolge im Loop

```
loop() — jede Iteration (~1 ms)
  │
  ├── btn.tick()                      OneButton polling
  ├── Encoder lesen                   alle 4 Schritte → EncoderRotated()
  ├── PAUSE_PIN prüfen                OLED "PAUSE" / Display wiederherstellen
  │
  ├── alle 5 ms:
  │     runmode_b77_tick()            ms-Zähler und Ring-Buffer-Akkumulatoren
  │     runmode_b77()                 gleitende Mittelwerte, STOP-Erkennung
  │     tape_restlauf()               Restzeit (Archimedes-Spirale, 1×/Umdrehung)
  │     tape_lenrout()                Absolutposition, Drift-Korrektur
  │
  ├── alle 30 ms:
  │     CalculatingDisplayParameters() Geschwindigkeit pollen
  │     RefreshDisplay()               OLED aktualisieren (wenn nicht gesperrt)
  │
  ├── alle 500 ms:
  │     isMoving aktualisieren         für Rew2Zero-Logik
  │
  ├── alle 1000 ms:
  │     operatingHoursCounter++        Betriebsstunden
  │     saveCounter++                  EEPROM-Speicher-Trigger
  │
  └── wenn saveCounter >= 4:
        SaveCounter()                  EEPROM schreiben + LED blinken
```

---

### Interrupt-Service-Routinen

```
CheckState()   — SENSOR_A CHANGE (rechte Spule, pin 3)
  Fallende Flanke: tlkurz = 0
  Steigende Flanke (wenn tlkurz >= 1 ms):
    → tright[] Ring-Buffer rotieren
    → umrabs ±1
    → counter ±1 (alle PULSES_COUNTER=2 Pulse)
    → counterMeters, counterSeconds akkumulieren
    → runflg++

CheckStateB()  — SENSOR_B CHANGE (linke Spule, pin 2)
  Fallende Flanke: trkurz = 0
  Steigende Flanke (wenn trkurz >= 1 ms):
    → tleft[] Ring-Buffer rotieren
    → umlabs ±1
    → runflg++

Richtung in beiden ISRs:
  dirForward = (digitalRead(DIRECTION_PIN) == HIGH)
```

---

## Anzeigemodi

Der Rotary Encoder schaltet zwischen drei Modi:

| Mode | Anzeige | Beschreibung |
|---|---|---|
| 0 | ` 12345` | Impulszähler (rechte Spule, 5-stellig, Vorzeichen) |
| 1 | ` 042'37` | Abgelaufene Zeit in Min'Sek (aus counterSeconds) |
| 2 | `R045'12` | **Restzeit** in Min'Sek (aus resttime) |

Im Modus 2, solange die Kalibrierung läuft:
```
CAL 24    ← Anzahl verbleibender Umdrehungen bis Kalibrierung abgeschlossen
```

---

## Bedienung

### Normalbetrieb

| Aktion | Funktion |
|---|---|
| Encoder drehen (rechts) | Nächster Modus (0→1→2→0) |
| Encoder drehen (links) | Vorheriger Modus (0→2→1→0) |
| Taste 1× drücken | Alle Zähler zurücksetzen (counter, counterSeconds, resttime, timeword) |
| Taste 2× drücken | Rewind-to-Zero starten (wenn counter > 0 oder timeword > 10 s) |
| Taste lang halten | Helligkeit einstellen (16 Stufen, blinkt beim Speichern) |
| Taste beim Einschalten | Betriebsstunden anzeigen |

### Rewind-to-Zero

Der Ablauf verwendet `timeword` (Bandposition in Sekunden) als Referenz — unabhängig von Spulengrösse und Geschwindigkeit:

```
Phase 0 (Grob):   timeword >= 120 s  → voller Rückspul-Speed
                   timeword <  120 s  → doStop() → Phase 1

Phase 1 (Fein):   Band steht → doRewind() (kurzer Burst)
                   timeword <   30 s  → doStop() → Phase 2

Phase 2 (Fertig): Band steht → rewindToZero = false, Display refresh

Sicherheit:       timeword <=   5 s  → Notstopp (jede Phase)
                   30 s kein Fortschritt → Abbruch (Bandklemmer / Riss)
```

---

## Kalibrierung der Restzeit

### Prinzip

Die Restzeit basiert auf der **Archimedes-Spirale** des Bandwickels:

```
r₀  = Innenradius rechte Spule bei Bandanfang
       = minrightl / (2π)

r₁  = aktueller Aussenradius linke Spule
       = (tleftmw × speed) / (2π)

Restzeit = π × (r₁² − r₀²) / (lend × timekorr × speed)

lend     = Banddicke in µm  (aus automatischer Kalibrierung)
timekorr = Driftkorrekturfaktor (startet 1.0, passt sich an)
```

### Kalibrierungsablauf

Die Kalibrierung startet automatisch beim **Bandanfang** (rechte Spule hat minimale Periode → `trightmw < mintime`):

```
1. Bandanfang erkannt:
   → minrightl = trightmw × speedActual    (Referenz-Innenradius)
   → calflg = 1
   → OLED zeigt "CAL xx" (Countdown)

2. Während UMMAX = 30 Umdrehungen:
   → Periodenwachstum der rechten Spule wird gemessen

3. Nach 30 Umdrehungen:
   → lend (Banddicke µm) wird aus Radiuszuwachs berechnet
   → calflg = 0
   → Werte werden in EEPROM gespeichert
   → Restzeit-Anzeige startet

4. Bei jedem weiteren Abspielen:
   → cc (Driftkorrektor × 1000) passt sich langsam an
   → Bereich: 900..1100  (±10% Korrekturfenster)
```

### Typische Banddicken

| Spulentyp | Banddicke | `lend` Wert |
|---|---|---|
| 26 cm Spule, Standardband | ~35 µm | 35 |
| 26 cm Spule, Langspielband | ~26 µm | 26 |
| 22 cm Spule | ~26–35 µm | je nach Band |

---

## EEPROM-Layout (Nano Every, 256 Bytes)

| Adresse | Grösse | Variable | Inhalt |
|---|---|---|---|
| 0 | 1 B | `EE_MODE` | Letzter Anzeigemodus (0/1/2) |
| 1 | 1 B | `EE_BRIGHTNESS` | Displayhelligkeit (0–16) |
| 2–5 | 4 B | `EE_COUNTER` | Impulszähler (int32) |
| 10–13 | 4 B | `EE_SECONDS` | Abgelaufene Zeit (float) |
| 20–23 | 4 B | `EE_OPHOURS` | Betriebsstunden (uint32) |
| 30–33 | 4 B | `EE_MINRIGHTL` | Kalibrierung: r₀-Metrik (int32) |
| 34–35 | 2 B | `EE_LEND` | Banddicke µm (int16) |
| 36–37 | 2 B | `EE_CC` | Driftkorrekturfaktor × 1000 (int16) |
| 254 | 1 B | `EE_INIT_A` | Erstinitialisierungs-Marker |
| 255 | 1 B | `EE_INIT_B` | Erstinitialisierungs-Marker |
| **gesamt** | **38 B** | | **218 B frei** |

EEPROM wird geschrieben:
- **Alle 4 Sekunden** wenn Band läuft (Zählerstand, Betriebsstunden)
- **Alle 10 Minuten** Betriebsstunden-Backup
- **Nach Kalibrierung** `lend`, `minrightl`, `cc`
- **Bei Helligkeitsänderung** und **Moduswechsel**

---

## Einstellbare Parameter (`config.h`)

### Display

| Parameter | Standardwert | Beschreibung |
|---|---|---|
| `HELLO_LOGO` | `true` | Startlogo anzeigen |
| `HELLO_TIMEOUT` | `3000` | Anzeigedauer Startbild [ms] |
| `BRIGHTNESS` | `128` | Anfangshelligkeit (0–255) |
| `FLIPDISPLAY` | `true` | Display 180° gedreht |
| `INVERTDISPLAY` | `false` | Invertiertes Display |
| `OLED_X_OFFSET` | `2` | Horizontale Verschiebung [px] |
| `OLED_Y_OFFSET` | `0` | Vertikale Verschiebung [px] |
| `I2C_ADDRESS` | `0x3C` | I²C-Adresse (0x3C oder 0x3D) |
| `DIGITS_COUNTER` | `5` | Stellen für Impulszähler |
| `DIGITS_MINUTES` | `3` | Stellen für Minuten (Zeitanzeige) |

### Sensorgeometrie

| Parameter | Standardwert | Beschreibung |
|---|---|---|
| `NUMSEGS` | `4` | Segmente pro Spulenumdrehung |
| `SCOPE` | `580` | Trimm-Wert für Meter-/Sekunden-Anzeige |
| `PULSES_COUNTER` | `2` | Pulse pro Zähler-Inkrement |

### Geschwindigkeit

| Parameter | Wert | Beschreibung |
|---|---|---|
| `SPEEDMIDDLE` | `190.5 mm/s` | 7.5 ips (19 cm/s) |
| `SPEEDLOW` | `95.25 mm/s` | 3.75 ips (9.5 cm/s) |

### Spulengrössen-Schwellwerte [ms/Umdrehung]

| Konstante | 26 cm | 22 cm | 18 cm |
|---|---|---|---|
| `TMIN` | 1900 | 1720 | 1050 |
| `TMID` | 3248 | 2692 | 2044 |
| `TMAX` | 4200 | 3450 | 2698 |

Quelle: EZS_UNO v1.5 (empirisch ermittelt bei 19 cm/s mit Revox/BASF-Bändern).

### Kalibrierung

| Parameter | Standardwert | Beschreibung |
|---|---|---|
| `UMMAX` | `30` | Umdrehungen für Banddicken-Kalibrierung |

### Rewind-to-Zero [Sekunden]

| Parameter | Standardwert | Beschreibung |
|---|---|---|
| `REW2ZERO_COARSE_SECS` | `120` | Umschalten auf Fein-Rückspulen |
| `REW2ZERO_FINE_SECS` | `30` | Endstopp-Schwelle |
| `REW2ZERO_FINAL_SECS` | `5` | Absoluter Notstopp |
| `REW2ZERO_MIN_SECS` | `10` | Mindest-Position für Auslösung |
| `REW2ZERO_TIMEOUT_SECS` | `30` | Abbruch bei Stillstand [s] |

---

## Bibliotheken

| Bibliothek | Version | Zweck |
|---|---|---|
| `OneButton` | aktuell | Einfach-/Doppel-/Langdruck-Erkennung |
| `Encoder` | aktuell | Rotary-Encoder-Auswertung (polling) |
| `EEPROM` | Arduino built-in | Persistente Datenspeicherung |
| `SSD1306Ascii` | greiman | OLED-Ansteuerung (kein U8g2, spart Flash) |
| `Wire` | Arduino built-in | I²C für OLED |

---

## Fehlersuche

### OLED zeigt `CAL xx` dauerhaft

Die Kalibrierung wurde noch nie erfolgreich abgeschlossen. Ursachen:
- Band liegt nicht am Anfang (rechte Spule noch nicht leer genug)
- `TMIN26` zu hoch eingestellt → `trightmw < mintime` wird nie erreicht
- Sensor auf rechter Spule defekt oder falsch ausgerichtet

Abhilfe: Band vollständig zurückspulen, dann PLAY drücken und 30 Umdrehungen abwarten.

### Restzeit springt oder ist ungenau

- `lend` falsch kalibriert → einmal komplett neu kalibrieren (Band zurück, PLAY)
- `SCOPE` zu weit von 580 entfernt → `counterSeconds` weicht ab von echter Zeit → `cc` kann nicht konvergieren
- Sensor-Segmente verschmutzt → unregelmässige Pulse → Ring-Buffer erhält Ausreisser

### Rewind-to-Zero stoppt zu früh / zu spät

Werte in `config.h` anpassen:
- Zu früh: `REW2ZERO_FINE_SECS` verkleinern
- Zu spät: `REW2ZERO_COARSE_SECS` verkleinern
- Bei 9.5 cm/s längere Bremswege: `REW2ZERO_COARSE_SECS` auf 180 erhöhen

### DIRECTION_PIN erkennt Richtung falsch

- Spannungsteiler prüfen: Pin 4 muss bei REW-aktiv < 0.8V haben
- Transistor-Variante bevorzugen (sicherere Pegelanpassung)
- Mit Multimeter messen: B77 REW-Ausgang = 24V aktiv, 0V inaktiv

---

## Versionsgeschichte

| Version | Datum | Änderungen |
|---|---|---|
| 1.0 | 2020 | Original DIYLab B77 TapeCounter |
| 2.0 | Okt. 2023 | Marc Stähli: B77 MKI Anpassungen, Encoder, Pause, Rewind-to-Zero |
| 2.1 | März 2026 | Restzeit-Berechnung (EZS-Port), Nano Every, Option-A Sensor-Umbau, timeword-basiertes Rewind-to-Zero, `sensor_isr.ino` / `runmode.ino` / `tape_calc.ino` neu |

---

## Quellen

- EZS_UNO v1.5 — Bandlängen- und Restzeitlogik (Archimedes-Spirale)
- [github.com/3KUdelta/TapeCounter_Revox_B77](https://github.com/3KUdelta/TapeCounter_Revox_B77) — Originalprojekt v2.0
- [old-fidelity-forum.de](https://old-fidelity-forum.de/thread-38940.html) — Community, gogosch (Geschwindigkeitsformel)
- Revox B77 Service Manual — Fernsteuer-Pinbelegung, Transistor Q1 Capstan PCB
