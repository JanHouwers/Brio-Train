# Project status — 2026-07-28

Both units run on the **Seeed XIAO ESP32-C5** and were both flashed with the current firmware on
28 July. The remote is wired; the **locomotive is not yet connected** — all documents are ready, but
no wire has been laid. The loco firmware has been seen running on the bare XIAO (the first part of
step 0 below; link LED, buzzer and the PWM test are still open).

The **radio link was demonstrated on 28 July**, in both directions, with both units on USB — see the
link test below.

## Build status — verified 2026-07-28

Both projects were built on the same platform used to flash the remote (`pioarduino 55.03.311`,
arduino-esp32 core 3.3.11, IDF 5.5.5, esptool 5.3.1):

| Project | Result | RAM | Flash |
|---------|--------|-----|-------|
| `firmware/loco` | SUCCESS, 0 warnings | 14.8 % (48,552 / 327,680) | 31.1 % (1,040,695 / 3,342,336) |
| `firmware/remote` | SUCCESS, 0 warnings | 14.8 % (48,592 / 327,680) | 31.1 % (1,037,887 / 3,342,336) |

Nothing comes out with `-Wall -Wextra` either. The two things that break fastest on a C5 —
`esp_wifi_set_band_mode()` and the new `esp_now_register_recv_cb` signature with
`esp_now_recv_info_t` — both compile fine.

## Both units flashed on 28 July — which is which

The two XIAOs can be told apart by their chip MAC. Useful, because they are identical and PlatformIO
will happily flash whichever one you point it at:

| Unit | Chip MAC | Port on 28 July |
|------|----------|-----------------|
| **loco** | `10:BD:A3:CF:A7:2C` | COM4 |
| **remote** | `10:BD:A3:CF:09:54` | COM3 |

Check with `pio device list` before every upload which MAC is attached — the port numbers change.
Second check via `device monitor`: the loco prints `pack ... mV ... | link ... armed ... fault ...`,
the remote prints `cell ... mV (... ratio 2.000 ...) | loco ... mV`.

The remote was flashed with `WiFi.setSleep(false)` included; after the reset it runs and reads the
cell at 4023 mV (pin 2011 mV, ratio 2.000). The `loco 8000 mV` in that line is still the startup
value of `locoPackMv` — no locomotive was powered, so no ack had arrived yet.

## Link test loco ↔ remote — passed on 28 July

Both XIAOs on USB simultaneously (COM3 + COM4), both monitors read side by side:

```
REMOTE  cell 4025 mV (...) | loco 7214 mV     LOCO  pack 7285 mV (D0 2428 mV) | link 1 armed 1 fault 0
REMOTE  cell 4025 mV (...) | loco 6735 mV     LOCO  pack 6765 mV (D0 2255 mV) | link 1 armed 1 fault 0
REMOTE  cell 4023 mV (...) | loco 2692 mV     LOCO  pack 3613 mV (D0 1204 mV) | link 1 armed 1 fault 0
REMOTE  cell 4024 mV (...) | loco    0 mV     LOCO  pack   42 mV (D0   14 mV) | link 1 armed 1 fault 0
```

What this proves:

- **Outbound.** `link 1` on the loco: broadcasts arrive that pass the `MAGIC` filter and the length
  check. So channel 1, 2.4 GHz and the `Packet` struct are correct on both sides.
- **Return path.** The remote no longer shows `loco 8000 mV` (the startup value) but a real value,
  and that value **tracks the wobbling D0 reading of the locomotive** — 7214 against 7285, 6735
  against 6765, and both drop to zero. That is not a coincidence: the unicast ack to
  `info->src_addr`, adding the peer, and the `Ack` struct all work. That also means the green link
  LED is lit (`lastAckMs` < 500 ms).
- **`armed 1`.** The locomotive saw `targetSpeed == 0` and released the neutral lockout. Which
  immediately means the joystick zero point at that boot **fell inside the deadband** — the scenario
  in point 1 below (skewed `joyCenter` → never `armed`) did not occur. Note: that is one observation
  at one boot, not a guarantee.

That the pack voltage reads nonsense and wobbles between 0 and 7.3 V is expected: D0 on the
locomotive is still floating. `lowBatt` flips back and forth as a result, and on the remote the red
LED blinks as soon as the reported value drops below 6400 mV.

**Not tested** (impossible without wiring or without extra diagnostics): horn, headlight, and whether
a stick deflection actually produces the right speed — the locomotive does not print `targetSpeed` /
`currentSpeed` anywhere yet.

## Bench test, locomotive — done on 28 July

The bare XIAO was flashed on **COM4** (chip MAC `10:bd:a3:cf:a7:2c`), all four segments with
`Hash of data verified`. The serial monitor shows the calibration line:

```
pack 2643 mV (D0 881 mV, ratio 3.000 x cal 1.000) | link 0 armed 0 fault 0
pack 0 mV (D0 0 mV, ...)      <- wobbles between 0 and 4355 mV
```

That is exactly the expected picture: **D0 is floating and drifts**, so `lowBatt` trips after three
readings and the headlight should blink at 300 ms. `link 0 armed 0` is right too — no remote was
switched on.

That drifting reading is itself the proof it was **the loco XIAO** and not accidentally the remote:
on the wired remote, D0 carries the VRy tap of the joystick, which would give a stable ~1.65 V (pack
would then read a constant ~4950 mV) instead of jumping between 0 and 4355 mV.

**Still to do** from step 0: switch the remote on to check the link LED, press the joystick for the
buzzer, and tie D0 to 3V3 temporarily to release the PWM on D4/D5.

## Two toolchain pitfalls — both solved, but worth remembering

1. **`UnicodeEncodeError` while uploading.** `pio run -t upload` dies with a `charmap` error as soon
   as esptool draws its progress bar: the console is on cp1252 and cannot render those characters.
   The flash is then half finished. Fix: `PYTHONIOENCODING=utf-8` in the environment, or `chcp 65001`
   before the command. This is the same cause as the message *"Firmware metrics can not be shown. Set
   the terminal codepage to utf-8"*.
2. **`build_dir` belongs in `[platformio]`, not in `[env:...]`.** Under the env section PlatformIO
   ignores it without complaining and builds inside the synced folder anyway — and then the build
   fails with `FileNotFoundError` on `.pio`, because the sync client has the directory open. Both
   files are correct now; the warning is included as a comment.

## Locomotive — build order

**Firmware first, wiring second.** Flash the bare XIAO C5 over USB before soldering anything: that
way you test the radio link, the headlight (D9) and the horn (D10) without a single solder joint.

0. **Bench test on the bare XIAO:** *(flashing + calibration line were done on 28 July, the rest not
   yet)*
   ```
   set PYTHONIOENCODING=utf-8
   pio run -t upload
   pio device monitor -b 115200
   ```
   Use PlatformIO Core ≥ 6.1.19 (the `pio` on PATH may be older; the one in
   `%USERPROFILE%\.platformio\penv\Scripts\` is usually current). Without `PYTHONIOENCODING` the
   upload aborts with a `UnicodeEncodeError` — see the pitfalls above.
   - D0 is floating, so the pack voltage reads nonsense and `lowBatt` trips → **the headlight blinks
     at 300 ms**. That confirms that branch works.
   - Switch the remote on: the **green link LED** must come on. That proves the whole radio chain
     (channel 1, 2.4 GHz, MAGIC, Packet/Ack).
   - Press the joystick → the buzzer on D10 must sound.
   - To see the motor drive: tie **D0 to 3V3 temporarily**. The firmware then reads 3.3 × 3.0 = 9.9 V,
     `lowBatt` clears and the PWM on D4/D5 is released.
1. **Measure the DRV8833 module while disconnected**, before anything else: `EEP↔VCC` and `ULT↔VCC`.
   - open or ±47 kΩ → good, carry on
   - near 0 Ω → J1 is closed. **Wick it off first**, otherwise L17 shorts the 3V3 of the XIAO against
     8.4 V as soon as you switch on.
2. **Set the buck to 5.0 V** with a bench supply, before mounting. Adjustable MP1584EN, not a fixed
   Mini-560.
3. **Lay wires L6–L29** (L18–L21 are dropped; that makes 25: L1–L17 + L22–L29).
4. **L1–L5** (pack, charger, star point) last, and only then fit the cells.
5. Verification measurements from the wire list — **motor still disconnected**.
6. Reflash, calibrate `VDIV_CAL` against the multimeter on VBAT_SW, and only then connect the motor.
7. Apply throttle with the locomotive on blocks. Free-running N20 = 60–100 mA, stalled 0.7–1 A.

## Protocol check loco ↔ remote — matches

Verified on 28 July, both sides side by side:

| Item | Loco | Remote |
|------|------|--------|
| `Packet` | 8 B, `packed`: magic/speed/flags/seq | identical |
| `Ack` | 7 B, `packed`: magic/packMv/seq | identical |
| `MAGIC` | `0xB210C0DE` | identical |
| Channel | 1 | 1 |
| Band | `WIFI_BAND_MODE_2G_ONLY` | `WIFI_BAND_MODE_2G_ONLY` |
| Role | receives broadcast, answers unicast to `info->src_addr` | sends broadcast, receives ack |

## What is deliberately not connected

nSLEEP and nFAULT are **not wired** — the goal is to avoid soldering on the module.

- **EEP** is tied to the 3V3 pin of the XIAO via **L17**. So the driver is always awake (~2 mA idle
  as long as SW1 is on). J1 stays open: VCC is 8.4 V here and the logic pins take 7 V maximum.
- **ULT** stays floating. No detection of short circuit or overheating. That is manageable: the OCP
  of the DRV8833 sits at 2–3.3 A and a stalled N20 only reaches ~1 A, so nFAULT would not have seen
  that anyway.
- **XIAO D1 and D8 stay free.** In firmware: `HAVE_NSLEEP 0` and `HAVE_NFAULT 0`.
- Protection now comes entirely from software: neutral lockout, link timeout, low-battery threshold
  (6600/6900 mV) and `if (!mayDrive) tgt = 0;`. That last line **must stay** — with EEP fixed high,
  `driverAwake` is always true and the ramp counter would otherwise keep running.

## Two things that easily go wrong

- **Two grounds.** PWR = star point at PACK− (pack−, buck IN−, DRV8833 GND, C3/C4). SIG = the GND pin
  of the XIAO (divider, LED1, BZ1). The only connection runs through the buck (L8 ↔ L10). **Do not
  add a second wire between them** — otherwise 1 A of motor current runs through your measurement
  ground.
- **SW1.** The SS12D00 from the original BOM is rated 0.3–0.5 A and has to carry 0.7–1 A here plus
  the inrush of C3. Use a switch rated ≥ 2 A or a P-MOSFET load switch, and a 2 A fuse/PTC directly
  after SW1.

## Next up: diagnostics + joystick ↔ motor speed

None of this is implemented yet — this is the analysis, not the code.

### Where things stand

The radio link is proven (see the link test above), so you can treat it as a given while calibrating.
What is missing is **visibility into the control chain itself**: neither unit prints what the joystick
produces or what the locomotive does with it. As long as that is the case, a locomotive that does not
move is indistinguishable from a locomotive that is being handed a speed of 0. So print first, drive
second.

### The chain stick → speed, and where it can bind

Five links, from the ADC in the remote to the worm gear in the locomotive:

1. **`joyCenter` comes from 32 readings at every boot** (`remote/src/main.cpp`) — and the stick has
   to be released at that moment. If it is not, the zero point is skewed. If it is off by more than
   the deadband, `speed` never becomes 0, the locomotive is **never released** into `armed` and it
   does nothing at all. That looks like a broken radio link while the link is perfectly fine.
   `joyCenter` is not printed anywhere, so right now this is invisible. **First thing to build in.**
2. **The scaling is symmetric, the joystick is not.** `readThrottle()` divides by a fixed 2048,
   regardless of where the centre actually sits. With `joyCenter` = 1900, reverse only reaches 93 %
   and the top 5 % of the forward travel is dead (the constrain clips it). Already listed under
   "still open"; scaling per direction against the real distance to 0 respectively 4095 fixes it.
3. **Deadband ±80 of 1000** = 8 % of the travel. Fine as a rest zone, but it stacks on top of point 4.
4. **There is no breakaway threshold.** A geared N20 only starts moving at roughly 15–25 % duty. The
   region between the deadband and that point therefore produces only current and whine, no movement
   — it feels like a dead zone of a quarter turn. A `MIN_DUTY` where the scale runs from the
   breakaway point to `MAX_DUTY` instead of from 0 makes the stick usable over its whole travel. That
   threshold has to be **measured**, not guessed: on blocks, raise the duty slowly until the wheel
   just turns.
5. **`MAX_DUTY` is a percentage of the pack voltage, not of a fixed motor voltage.** 75 % of 8.4 V is
   6.3 V, but 75 % of 6.6 V (just above the low-battery threshold) is 4.95 V. So the speed at the
   same stick position drops ~21 % as the pack drains. The locomotive **already measures `packMv`**,
   so compensating costs no extra wire: derive the duty from a desired motor voltage. Watch the upper
   limit — never above 80 % duty, and with a full pack never above the 6.3 V the N20 is set up for.

### Hard limitation: speed cannot be measured with the current hardware

There is **no encoder on the N20**, and back-EMF measurement is not possible here: of the header pins
only **D0 (GPIO1) is on ADC1**, and it is taken by the pack voltage divider. The C5 has no ADC2. So
"speed" is by definition a derivative of duty and motor voltage, supplemented by what you determine
on blocks by eye or with a tacho app. Measuring it properly is a **hardware change** — an optical or
hall sensor on a free GPIO (D1 and D8 are free, see `HAVE_NSLEEP 0` / `HAVE_NFAULT 0`) — and then the
pin assignment has to go past the wire list again.

### What could be built in

**Remote:**
- Extend the calibration line with the raw ADC value, `joyCenter`, and `v` after scaling. Without
  those three, calibrating is guesswork.
- Calibration mode: hold the joystick pressed at boot → measure the end stops and centre and write
  them to NVS (`Preferences`), so the zero point no longer depends on whether the stick happened to
  be released at boot.
- Scale per direction (point 2).

**Locomotive:**
- Extend the calibration line with `targetSpeed`, `currentSpeed` and the actual duty, plus
  `armed`/`mayDrive`/`lowBatt`/`faultShutdown` as separate flags. Right now you only see
  `link armed fault`.
- `MIN_DUTY` and voltage compensation (points 4 and 5), only after the breakaway point is measured.

**Telemetry — mind the trap:** extending the `Ack` with speed and status flags is the clean way to
see on the remote what the locomotive is doing, but **both sides filter on `len != sizeof(...)`**. If
you change the struct, loco and remote must be reflashed in the same pass, otherwise the link goes
silent without any message — the packets are simply discarded.

## Still open, apart from the locomotive

- **Headlight LED, 47 Ω.** With a white LED (Vf ≈ 3.0–3.2 V) only 0.1–0.3 V is left across the
  resistor; the LED itself becomes the limiter and the brightness is not well defined. Put a red LED
  in there by mistake and you draw ~30 mA from the GPIO. More robust: drive the headlight from the
  5 V rail through an NPN (BC547, 1 kΩ base resistor from D9) with **180 Ω** in series.
- **Green LED on the remote, 470 Ω.** 470 Ω is right for the red LED (Vf ≈ 1.9 V → 3 mA), but a
  bright green InGaN LED has Vf ≈ 3.0–3.2 V and barely lights up. Use a green one with Vf ≤ 2.2 V, or
  220 Ω.
- `readThrottle()` in the remote firmware scales both directions equally; with a joystick whose
  centre is not exactly at half, forward and reverse are not symmetric. The fix (scale per direction)
  has been discussed but is not in the code yet.
- **The locomotive enclosure is not finished** — the CAD in [`../cad/`](../cad/) is the current
  source, but there are no print-ready STLs in the repository yet.
