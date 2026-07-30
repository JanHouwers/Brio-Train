# Locomotive — wire list rev D (XIAO ESP32-**C5**)

Goes with [`hardware/wiring-loco.svg`](../hardware/wiring-loco.svg) and
[`firmware/loco/src/main.cpp`](../firmware/loco/src/main.cpp) (PlatformIO). Successor to rev C,
which was for the ESP32-S3. **The wiring itself has not changed** — L1 through L29 still run to the
same components. What changed is **which XIAO label** each signal sits behind, plus the buck
setting.

> Both ends now run on a C5. The remote is documented in [`wiring-remote.md`](wiring-remote.md).

## Read this first — four ways to get it wrong

1. **The DRV8833 module used here (black, 6+6 pins) has J1 and a 47k pull-up to VCC.** And on this
   board "VCC" is simply VM, so **8.4 V** — the DRV8833 has no separate logic supply. Connect EEP or
   ULT without removing those and you put 8.4 V on a 3.3 V GPIO. See the separate section
   **"Your module"** below; that is the first thing to check.
2. **The slide switch from the BOM is too light.** The SS12D00 is rated 0.3–0.5 A; the full motor
   current runs through it here (0.7–1 A stalled) plus the inrush of C3. Use ≥2 A, or have SW1 drive
   a P-MOSFET load switch. Plus a 2 A fuse directly after it.
3. **The buck must be set to 5.0 V, and it must be an adjustable type.** See below — this changed
   compared to rev C.
4. **The cell bay is antiparallel.** Nothing stops a reversed cell. Mark the polarity and measure
   PACK+ ≈ 2× MID before switching on for the first time.

## Your module — read this before connecting anything

The silkscreen on this board is mangled English. Decoded:

| Silkscreen | Means | In this design |
|------------|-------|----------------|
| `IN1` `IN2` | AIN1, AIN2 | channel A, from D4 and D5 |
| `IN3` `IN4` | BIN1, BIN2 | channel B — **do not connect** |
| `OUT1` `OUT2` | AOUT1, AOUT2 | to the motor |
| `OUT3` `OUT4` | BOUT1, BOUT2 | **do not connect** |
| `EEP` | n**SLEEP** | from D1 |
| `ULT` | nFA**ULT** | to D8 |
| `VCC` | **VM** — there is no separate logic supply | 8.4 V |
| `GND` | ground | PWR star point |
| `J1` | solder bridge next to EEP, silkscreen "en sleep" | connects EEP to VCC |

**That VCC is the same net as VM is the whole problem.** The DRV8833 has one supply pin. Everything
on this board tied to VCC therefore sits at pack voltage, not 3.3 V. On the front there is a `473`
(47 kΩ) and a `472` (4.7 kΩ); the 4.7k belongs to the green LED, the 47k is almost certainly the
pull-up on ULT.

### Measuring (module disconnected, nothing hooked up)

| Measurement | Result | What you do |
|-------------|--------|-------------|
| **EEP ↔ VCC** | open or ± 47 kΩ | wire from XIAO **3V3** to EEP — done |
| | near 0 Ω | J1 is closed: wick it off first, only then connect 3V3 |
| **ULT ↔ VCC** | does not matter | ULT stays floating |

### Chosen approach: do not desolder

`src/main.cpp` is therefore set to:

```c
#define HAVE_NSLEEP  0    // 1 = EEP on D1, only after J1 is removed
#define HAVE_NFAULT  0    // 1 = ULT on D8, only after the 47k is removed
```

**Leave ULT floating.** There is nothing else to do.

**EEP does have to be held high** — otherwise the internal 500 kΩ pulldown drags it low and the
driver stays asleep; the motor will never move.

> **Do not use J1 for this.** It connects EEP to VCC, and VCC on this board is VM: 8.4 V with a full
> pack. The logic pins of the DRV8833 — including nSLEEP — have an **absolute maximum of 7 V**
> (TI datasheet). Closing J1 therefore puts 1.4 V too much on it, precisely when the pack is full.

**Instead, run a single wire from the 3V3 pin of the XIAO to EEP.** That is L17 in the wire list,
only from 3V3 instead of from D1. nSLEEP then sits at 3.3 V, well within spec, the driver is
permanently enabled, and you do not have to solder anything on the module.

First measure **EEP ↔ VCC** with the module disconnected:

- **open, or ± 47 kΩ** → wire from 3V3 to EEP. Done. (With a 47k to VCC, 108 µA flows into the 3V3
  rail — negligible.)
- **near 0 Ω** → J1 was closed at the factory. Do **not** connect 3V3, because that would short the
  XIAO 3V3 rail against the pack. Wick the bridge off first — removing a solder bridge is a lot
  easier than removing an 0603 — and then run the wire to 3V3.

At boot this is safe: EEP goes high as soon as 3V3 is present, but IN1/IN2 are still high-Z at that
point and the DRV8833 has internal pulldowns on them. Both low = coast.

What this approach costs you:

- **without nSLEEP**: the driver is always on (~2 mA idle as long as SW1 is on) and you cannot clear
  a fault latch. The failsafe still works: on signal loss the firmware drives both inputs low and the
  locomotive coasts to a stop.
- **without nFAULT**: you do not see short circuits or overheating. That is less bad than it sounds —
  see the note at the bottom about what nFAULT does and does not catch.

The module already has **10 µF** on VCC (the orange block marked `106C`). C3/C4 are still worth it:
10 µF is little for the peak current of an N20.

The green LED on the module hangs off VCC through 4.7 kΩ and therefore burns continuously as long as
SW1 is on — about 1.4 mA out of your pack. If you do not want that, remove the 4.7 kΩ.

## Why the pins differ from the S3

The ESP32-C5 has exactly **one ADC unit**: ADC1, six channels, on GPIO1 through GPIO6. There is no
ADC2 — the Wi-Fi/ADC2 conflict of the S3 does not exist on this board. But of the **header pins** of
the XIAO C5, only **D0 (GPIO1)** is on that ADC. So the pack voltage divider has to go there.

That the motor control moves as a result is no problem: **AIN1 and AIN2 of the DRV8833 are digital
inputs.** Speed is the duty cycle of a 20 kHz square wave from the LEDC hardware, not an analog
voltage. The same holds for nSLEEP (output) and nFAULT (open-drain input). So the DRV8833 needs zero
ADC pins; the pack voltage is the only analog measurement in the whole locomotive.

**Two header pins stay empty.** Strapping pins on the C5 are GPIO2, GPIO7, GPIO25, GPIO27 and
GPIO28. On the header those are **D3 (GPIO7)** and **D2 (GPIO25)**. A strapping pin must have no
external pull-up or pulldown at reset — the chip reads that level as a boot option. That is why D2
and D3 stay unwired. In this build **D1 and D8 also stay free**: those were nSLEEP and nFAULT, and
they are not connected.

**D6 and D7 (GPIO11/GPIO12) are the UART.** Keep them free: that is where the boot log comes out and
where you hang a USB-TTL adapter when USB-CDC lets you down.

## Pinout, C5 versus S3

| Label | Net | S3 (rev C) | C5 (rev D) | Note |
|-------|-----|-----------|-----------|------|
| D0 | VSENSE | GPIO1 = AIN1 | **GPIO1** | ADC1_CH0 — the only ADC on the header |
| D1 | ~~nSLEEP~~ | GPIO2 = AIN2 | **GPIO0** | **free** — nSLEEP is not driven |
| D2 | — | GPIO3 = headlight | **GPIO25** | **strapping — leave free** |
| D3 | — | GPIO4 = horn | **GPIO7** | **strapping — leave free** |
| D4 | AIN1 | GPIO5 = VSENSE | **GPIO23** | ordinary GPIO, PWM through the GPIO matrix |
| D5 | AIN2 | GPIO6 = nSLEEP | **GPIO24** | idem |
| D6/D7 | — | GPIO43/44 | **GPIO11/12** | UART — keep free |
| D8 | ~~nFAULT~~ | GPIO7 | **GPIO8** | **free** — nFAULT is not read |
| D9 | HEADLIGHT | — | **GPIO9** | ordinary GPIO |
| D10 | HORN | — | **GPIO10** | ordinary GPIO |

Sources: `variants/XIAO_ESP32C5/pins_arduino.h`, the ESP32-C5 datasheet (ADC1 = GPIO1…6) and the
Espressif hardware design guidelines / ESP-IDF GPIO page for the strapping pins.

## The buck — this changed compared to rev C

Rev C said "MP1584EN at **4.6 V**". That no longer holds now that a C5 is fitted, and honestly it was
already tight on the S3. The 5V pin of the XIAO feeds the on-board **SGM40567**, and that wants to
see roughly 4.5–5.5 V to consider its input valid. At 4.6 V you are on the edge: dip below it
briefly and the board falls back to the BAT pads — and nothing is connected there on the locomotive,
so the XIAO switches off.

**Set the buck to 5.0 V.** That it must be an *adjustable* MP1584EN and not a fixed Mini-560 still
stands, but for a different reason than given earlier: the Mini-560 no longer reaches 5 V below about
6.5 V input, and that is *above* the low-battery stop. The MP1584EN goes to 100 % duty and holds
5.0 V down to about 5.4 V input — well below the 6.6 V at which the firmware has already stopped.

## Order of assembly

1. L6–L31 first: everything except the pack.
2. **L1–L5 (pack, charger, star point) last**, and only then fit the cells.
3. Verification measurements at the bottom, and only then connect the motor.

Insulate each board (Kapton tape) before it goes in — everything ends up packed together inside the
chassis and body, with no exposed conductors allowed anywhere.

## Two grounds — this is not a detail

| Symbol | What | What hangs off it |
|--------|------|-------------------|
| **PWR** | star point at PACK− — one physical solder joint | L5 (pack−), L8 (buck IN−), L11 (DRV8833 GND), L13 (C3/C4 −) |
| **SIG** | the GND pin of the XIAO | L23 (LED1 cathode), L25 (BZ1), L27 (R4 + C1) |

The only connection between them runs **through the buck**: L10 (OUT− → XIAO GND) and L8 (IN− → star
point). **Do not add a second wire between them** — that creates a loop 1 A of motor current can run
through, and your ADC zero shifts with the throttle.

## Wire list

| # | Net | From | To | Colour | Wire |
|---|-----|------|----|--------|------|
| L1 | PACK+ | pack positive contact | U1 charger **B+** | red | AWG20 |
| L2 | PACK+ | branch off L1 | SW1 **pin 2** (middle) | red | AWG20 |
| L3 | MID | jumper between the cells | U1 charger **BM** | yellow | AWG22 |
| L4 | PACK− | pack negative contact | U1 charger **B−** | black | AWG20 |
| L5 | PWR | branch off L4 | **PWR star point** | black | AWG20 |
| L6 | VBAT | SW1 **pin 1** | U2 buck **IN+** | red | AWG20 |
| L7 | VBAT | branch off L6 | U4 pin **VCC** (= VM) | red | **AWG20** |
| L8 | PWR | U2 buck **IN−** | **PWR star point** | black | AWG22 |
| L9 | 5V | U2 buck **OUT+** (5.0 V) | U3 XIAO **5V** | red | AWG24 |
| L10 | SIG | U2 buck **OUT−** | U3 XIAO **GND** | black | AWG24 |
| L11 | PWR | U4 DRV8833 **GND** | **PWR star point** — its own wire | black | **AWG20** |
| L12 | VBAT | C3/C4 positive side | **VM pin** of the DRV8833, < 2 cm | red | AWG22 |
| L13 | PWR | C3/C4 negative side | **GND pin** of the DRV8833, < 2 cm | black | AWG22 |
| L14 | VBAT | branch off L6/L7 | R3 top side (divider) | red | AWG26 |
| L15 | AIN1 | U3 XIAO **D4 / GPIO23** | U4 pin **IN1** | blue | AWG26 |
| L16 | AIN2 | U3 XIAO **D5 / GPIO24** | U4 pin **IN2** | blue | AWG26 |
| L17 | EEP high | U3 XIAO **3V3** | U4 pin **EEP** — see above | orange | AWG26 |
| ~~L18~~ | ~~nFAULT~~ | — | — | — | **dropped** — ULT stays floating |
| ~~L19~~ | ~~nFAULT~~ | — | — | — | **dropped** — no R5/C2 block |
| ~~L20~~ | ~~3V3~~ | — | — | — | **dropped** — no R5/C2 block |
| ~~L21~~ | ~~SIG~~ | — | — | — | **dropped** — no R5/C2 block |
| L22 | HEADLIGHT | U3 XIAO **D9 / GPIO9** | R1 47 Ω → LED1 **anode** | blue | AWG26 |
| L23 | SIG | LED1 **cathode** (short leg) | **SIG — GND pin of the XIAO** | black | AWG26 |
| L24 | HORN | U3 XIAO **D10 / GPIO10** | R2 100 Ω → BZ1 | blue | AWG26 |
| L25 | SIG | BZ1 other terminal | **SIG — GND pin of the XIAO** | black | AWG26 |
| L26 | VSENSE | divider tap (R3/R4 + C1) | U3 XIAO **D0 / GPIO1** | blue | AWG26 |
| L27 | SIG | R4 bottom side + C1 | **SIG — GND pin of the XIAO** | black | AWG26 |
| L28 | MOTOR A | U4 pin **OUT1** | M1 terminal A | purple | **AWG20** |
| L29 | MOTOR B | U4 pin **OUT2** | M1 terminal B | purple | **AWG20** |
| L30 | VBUS | J2 USB-C socket, **+** lead | U1 charger, **charge input +** (5 V) | red | AWG24 |
| L31 | VBUS− | J2 USB-C socket, **−** lead | U1 charger, **charge input −** | black | AWG24 |

**Do not connect:** SW1 pin 3, DRV8833 **IN3, IN4, OUT3, OUT4** (channel B) and **ULT**, XIAO
**D1, D2, D3, D6, D7, D8** and the BAT pads of the XIAO.

That makes **27 wires**: L1–L17, L22–L29 and L30–L31.

### L30/L31 — the external charge socket

The USB-C port on the charger board itself is not brought out through a wall. Instead, a separate
2-wire USB-C female connector (J2, see [`bom.md`](bom.md)) is mounted where you want the socket and
its two leads are soldered to the charge input of U1. Points to watch:

- **Polarity.** The red lead is VBUS (+5 V), the black one is ground. Reversed, you feed 5 V
  backwards into the charger.
- **CC resistors.** A 2-wire socket only carries VBUS and GND. It needs the two 5.1 kΩ CC
  pull-downs built in, otherwise a USB-C-to-USB-C charger will deliver nothing at all — the "3 A fast
  charge" type in the BOM has them. A USB-A-to-USB-C cable works either way.
- **This bypasses the charger's own USB-C connector**, not its charge circuitry: balancing, current
  limit and termination are unchanged.
- The wiring diagram `hardware/wiring-loco.svg` still shows the charger with its own USB-C port and
  does not yet show L30/L31.

**No wires, but do solder these:**

- **C5 10 nF** directly across the two motor terminals. Do not go larger: 100 nF already burns
  0.14 W in the H-bridge at 20 kHz PWM and produces current spikes of ~10 A through the bridge.
- ~~R6 100 kΩ pulldown on nSLEEP~~ — dropped: EEP is held hard at 3V3 via L17.

## The pack voltage measurement

External divider **R3 200 kΩ (top) / R4 100 kΩ (bottom)** on VBAT_SW, tap to D0, with **C1 100 nF**
from tap to SIG ground. Ratio 3.0 → 8.4 V becomes 2.80 V at the ADC.

Source impedance is 200k ∥ 100k = 67 kΩ, well above the <10 kΩ the SAR ADC wants to see. C1 supplies
the sampling charge locally, and the firmware additionally averages over 16 readings. Unlike the
remote, the tap here **is reachable** with a multimeter — that is your calibration point.

The firmware prints every 2 seconds:

```
pack 7840 mV (D0 2613 mV, ratio 3.000 x cal 1.000) | link 1 armed 1 fault 0
```

Calibration: multimeter on **VBAT_SW relative to SIG ground**, compare against the printed value, and
set `VDIV_CAL = multimeter / printed` in `main.cpp`. Cross-check with the tap: it must be exactly one
third of VBAT_SW.

**The on-board cell divider of the XIAO does nothing here.** It measures the BAT pads, and nothing is
connected to those on the locomotive. The firmware therefore drives GPIO26 low explicitly so the load
switch stays closed and the enable input does not float.

## Checks before fitting the cells

1. **Measure the module while disconnected**: EEP↔VCC and ULT↔VCC, per the table in "Your module"
   above.
2. Set the buck to **5.0 V** with a bench supply on the input, before mounting it.
3. Continuity check SW1 pin 1 → buck IN+ and → DRV8833 VM with SW1 on; **no** continuity with SW1
   off.
4. Measure the divider **before L14 is soldered**: R3+R4 ≈ 300 kΩ.
5. Verify there is **no** continuity between the PWR star point and the GND pin of the XIAO other
   than through the buck.
6. Fit the cells with the polarity verified; measure PACK+ ≈ 2× MID.

## Checks after switching on — motor still disconnected

Disconnect L28/L29 and measure without the motor first.

| Test point | Expected | Cross-check |
|------------|----------|-------------|
| VBAT (SW1 pin 1) | 6.0–8.4 V | pack voltage |
| buck OUT+ | 4.95–5.05 V | as adjusted |
| XIAO 3V3 | 3.25–3.35 V | internal LDO |
| divider tap (D0) | exactly one third of VBAT | 8.4 V → 2.80 V |
| printed pack value | within ~2 % of the multimeter | otherwise adjust `VDIV_CAL` |
| EEP pin of the module | 3.3 V | held high via L17 — driver always awake |
| ULT pin of the module | 8.4 V (= VCC) | 47 kΩ pull-up on the module, not connected |
| XIAO D1 and D8 | floating, no wire | both pins stay free |
| OUT1/OUT2 at rest | both low (brake) | IN1 = IN2 = 0 with the motor stopped |

Then connect the motor and apply throttle carefully with the locomotive on blocks. A free-running N20
draws 60–100 mA, stalled 0.7–1 A.

## Parts not yet in the BOM

| Part | Value | Reason |
|------|-------|--------|
| C1 | 100 nF ceramic (X7R) | 67 kΩ source impedance of the divider versus the <10 kΩ the ADC wants |
| C3 | 100 µF low-ESR electrolytic, ≥ 16 V | HF decoupling of the H-bridge, 2 cm loop. **Not** a brownout buffer: at 1 A, 100 µF holds the voltage for only ~100 µs per volt |
| C4 | 100 nF ceramic | in parallel with C3 |
| C5 | 10 nF ceramic | noise suppression across the motor terminals |
| — | switch rated ≥ 2 A | replaces the SS12D00 |
| — | fuse or PTC, 2 A | directly after SW1 |
| — | buck MP1584EN | adjustable, set to **5.0 V** |
| — | wire AWG20/22/24 | the BOM only lists AWG26 |

## Firmware — `firmware/loco/` (PlatformIO)

Same setup as the remote: `platformio.ini` with the pioarduino fork (`55.03.311`, board
`seeed_xiao_esp32c5`) and `src/main.cpp`. Build and flash:

```
pio run -t upload
pio device monitor -b 115200
```

What changed compared to rev C:

- pins by **label** (D0…D10) instead of bare GPIO numbers, with the C5 mapping in the comments
- `esp_wifi_set_band_mode(WIFI_BAND_MODE_2G_ONLY)` before `esp_wifi_set_channel()`
- the on-board cell divider is disabled (GPIO26 low)
- `packMv()` averages over 16 readings and has a `VDIV_CAL` trim factor
- a calibration line over USB, so you can check the measurement instead of guessing

Unchanged from rev C, and still important:

- IN1/IN2 are driven low as the very first thing in `setup()`: the motor cannot twitch at boot. The
  hardware nSLEEP can no longer enforce that, so this is now the only boot protection
- **neutral lockout**: after power-up or signal loss the locomotive only drives once the throttle
  stick has been at centre one time
- **`currentSpeed` is held at zero while driving is not allowed** — with `HAVE_NSLEEP 0` the line
  `if (!mayDrive) tgt = 0;` does that work; otherwise the ramp counter keeps running during a fault
  and the locomotive jumps to full duty in a single loop on recovery
- fault counter with debounce, retry after 1 s, permanent shutdown after 5 faults in 10 s
- hysteresis on the low-battery stop (6600 / 6900 mV, 3 consecutive readings)
- `ledcWriteTone()` only on a change; `WiFi.setSleep(false)`

**What you give up by not connecting nFAULT:** the overcurrent protection of the DRV8833 sits at
2–3.3 A and a stalled N20 stays around 1 A. So nFAULT catches short circuits and overheating, not a
stalled locomotive — precisely the case you expect with a toy train. The firmware already covers that
with the low-battery threshold and the link timeout. What remains: a genuine short in the motor
wiring will show up as a hot driver, not as a fault message.
