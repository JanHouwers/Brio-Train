# Bill of Materials

Current hardware revision: **loco rev D / remote rev F**, both on the Seeed **XIAO ESP32-C5**.
Component designators (R1, C3, SW1, …) match [`wiring-loco.md`](wiring-loco.md) and
[`wiring-remote.md`](wiring-remote.md) and the two wiring diagrams in [`../hardware/`](../hardware/).

> Earlier revisions of this project used the XIAO ESP32-S3. The C5 has a different pinout and only
> one ADC unit; do not mix the two. See the wire lists for the details.

## Controllers and cells

| # | Part | Notes |
|---|------|-------|
| 2 | Seeed XIAO ESP32-C5 | 1× loco, 1× remote (21 × 17.5 mm). Requires arduino-esp32 core ≥ 3.3 |
| 2 | XTAR 14500, 3.7 V 1200 mAh, protected | Ø14.3 × 50.5 mm — loco pack, 2S (6.0–8.4 V) |
| 1 | XTAR 26650, 3.6 V 6000 mAh, protected, button top | Ø26.5 × 69.3 mm — remote, 1S |

Protected 14500 cells are 50.5 mm — 0.5 mm longer than an AA. The printed bay accounts for this; an
off-the-shelf AA holder may be too tight.

## Locomotive

| # | Ref | Part | Spec / example | Why |
|---|-----|------|----------------|-----|
| 1 | M1 | N20 gearmotor | 6 V, ~150 rpm, metal gears, 3 mm D-shaft, body 12 × 10 × 24 mm | Drive. 2 A protected cells handle N20 stall (~0.7–1 A) fine |
| 1 | U4 | DRV8833 module | Dual H-bridge breakout, ~18 × 15 mm, Vmax 10.8 V | Motor driver, runs directly off the 2S pack. One channel used |
| 1 | U1 | 2S USB-C Li-ion charge module | IP2326-based, "2S 8.4 V USB-C balance charging board" | Built-in USB-C charging including balancing of both cells |
| 1 | U2 | Buck converter, **adjustable** | MP1584EN set to **5.0 V**, ≥1 A | 2S pack → 5V pin of the XIAO. **Not** a fixed Mini-560 — see below |
| 4 | — | AA battery contacts | 2× spring (negative) + 2× button plate (positive), Keystone 5201/5203 style or from a scrap AA holder | Press into the printed battery bay |
| 1 | SW1 | Slide switch, **≥ 2 A** | 1P2T, 3–4 mm travel | Master power. The SS12D00 from earlier revisions is too light — see below |
| 1 | — | Fuse or PTC, 2 A | — | Directly after SW1 |
| 1 | LED1 + R1 | White LED 5 mm + 47 Ω | LED with Vf ≤ 2.9 V | Headlight |
| 1 | BZ1 + R2 | Passive piezo buzzer + 100 Ω | 12 mm disc or 9 mm passive buzzer | Horn |
| 2 | — | O-ring | 18 × 2 mm NBR | 1× tyre on the driven wheel, 1× drive belt motor→wheel |
| 1 | — | Steel rod | Ø3 × 32 mm | Front axle (press-fit wheels, spins in the chassis) |
| 2 | — | M3 × 12 screw + washer | — | Rear stub axles; both rear wheels free-spin, the right one is belt-driven |
| 2 | — | Neodymium disc magnet | Ø10 × 3 mm | Couplers front and rear. Check polarity against a Brio wagon before gluing |
| 1 | R3 | Resistor 200 kΩ | 1 % | Pack voltage divider, top leg |
| 1 | R4 | Resistor 100 kΩ | 1 % | Pack voltage divider, bottom leg (ratio 3.0 → D0) |
| 1 | C1 | 100 nF ceramic, X7R | — | Across the divider tap to SIG ground. The divider is 67 kΩ source impedance; the SAR ADC wants < 10 kΩ |
| 1 | C3 | 100 µF low-ESR electrolytic, ≥ 16 V | — | HF decoupling of the H-bridge, loop < 2 cm |
| 1 | C4 | 100 nF ceramic | — | In parallel with C3 |
| 1 | C5 | 10 nF ceramic | — | Across the motor terminals. Do not go larger: 100 nF burns 0.14 W at 20 kHz PWM |
| — | — | Wire | AWG20 (power/motor), AWG22, AWG24, AWG26 (signals) | See the wire list for which gauge goes where |
| — | — | Heat-shrink | — | — |

## Remote

| # | Ref | Part | Spec / example | Why |
|---|-----|------|----------------|-----|
| 1 | U1 | TP4056 USB-C charge module | With protection (DW01+FS8205), 1 A | Built-in USB-C charging of the 26650 (~7 h full charge) |
| 1 | J1 | Joystick module | KY-023 dual axis with push switch, 34 × 26 mm | Throttle (Y axis) fwd/rev, push = horn |
| 1 | SW1 | Slide switch | SS12D00 (fine here — no motor current) | Master power |
| 1 | LED1 + R1 | Green LED 3 mm + 470 Ω | **Vf ≤ 2.2 V**, otherwise use R1 = 220 Ω | Link status |
| 1 | LED2 + R2 | Red LED 3 mm + 470 Ω | — | Low battery |
| 1 | R5 | Resistor 100 kΩ | — | External pull-up on D1 (JOY_SW). The internal pull-up disappears in deep sleep |
| 1 | C2 | 100 µF low-ESR electrolytic (≥ 6.3 V) | — | ESP-NOW draws 250–350 mA peaks; without it a nearly empty cell browns the XIAO out |
| 1 | C3 | 100 nF ceramic | — | In parallel with C2 |
| 4 | — | Screws M3 × 12 self-tapping | — | Enclosure halves |
| 2 | — | Battery contacts | 26650/C-cell spring + plate, or solder tabs | Printed battery bay |
| 1 | — | Foam pad 1–2 mm | EVA or double-sided foam tape, ~10 × 10 mm | Between the top-shell pillar and the XIAO RF shield (clamps the board) |

**Dropped compared to the S3 revision:** the external battery divider (R3, R4) and its filter cap
(C1), plus wires W5, W8 and W9. On the C5 the battery sense runs through the divider on the XIAO
itself (GPIO6, enabled by GPIO26), which is not reachable from the header.

## Component notes

- **Do not run the N20 at the full 8.4 V** — the firmware caps PWM duty at 75 % (≈6.3 V effective).
  This is by design; do not raise `MAX_DUTY` above 80.
- **The buck must be adjustable and set to 5.0 V.** The 5V pin of the XIAO feeds the on-board
  SGM40567, which wants to see ~4.5–5.5 V. A fixed Mini-560 no longer reaches 5 V below about 6.5 V
  input, and that is *above* the low-battery stop; the MP1584EN holds 5.0 V down to about 5.4 V in.
- **SW1 in the locomotive carries the full motor current** (0.7–1 A stalled) plus the inrush of C3.
  The SS12D00 is rated 0.3–0.5 A — use a switch rated ≥ 2 A, or have SW1 drive a P-MOSFET load
  switch. Add a 2 A fuse directly after it.
- **The 2S charge module charges through the balance tap (B−, BM, B+).** The midpoint (BM) is the
  junction between the two 14500 cells — it must be wired even though the cells are individually
  protected. Never charge this pack through a plain 1S board.
- **The XIAO in the remote is powered from its BAT pads** (cell voltage). Its own USB port then also
  trickle-charges the cell at ~100 mA if plugged in — harmless, but the TP4056 USB-C port is the
  intended charge port, and with USB plugged in the battery reading always says "full".
- **Headlight LED, 47 Ω:** with a white LED (Vf ≈ 3.0–3.2 V) only 0.1–0.3 V is left across the
  resistor; the LED itself becomes the limiter and brightness is not well defined. Put a red LED
  there by mistake and you draw ~30 mA from the GPIO. More robust: drive the headlight from the 5 V
  rail through an NPN (BC547, 1 kΩ base resistor from D9) with **180 Ω** in series.
- **Green link LED, 470 Ω:** correct for a red LED (Vf ≈ 1.9 V → 3 mA), but a bright green InGaN LED
  has Vf ≈ 3.0–3.2 V and barely lights up. Use a green LED with Vf ≤ 2.2 V, or 220 Ω.
