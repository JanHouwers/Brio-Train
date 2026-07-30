# Brio-Train — a remote-controlled BRIO-compatible locomotive

A 3D-printed, battery-powered locomotive for BRIO-style wooden track, with a matching handheld
remote. Both units use a **Seeed XIAO ESP32-C5** and talk over **ESP-NOW**; both charge over USB-C.

| | Locomotive | Remote |
|---|---|---|
| Controller | XIAO ESP32-C5 | XIAO ESP32-C5 |
| Power | 2S, 2× XTAR 14500 protected (6.0–8.4 V) | 1S, XTAR 26650 protected |
| Charging | IP2326 2S USB-C balance charger | TP4056 USB-C |
| Drive | N20 6 V 150 rpm → DRV8833, O-ring belt to one rear wheel | — |
| Controls | headlight, horn | KY-023 joystick (throttle + horn), link LED, battery LED |
| Link | ESP-NOW broadcast, channel 1, 2.4 GHz, MAGIC-filtered | ESP-NOW, 25 Hz, telemetry ack back |

Speed is roughly 16 cm/s; the firmware caps the PWM duty at 75 % so the 6 V N20 survives the 8.4 V
pack.

> **Build status:** the remote is wired and working. The locomotive is documented and its firmware
> runs on the bare board, but **it is not wired up yet**. The radio link has been demonstrated in
> both directions. See [`docs/status.md`](docs/status.md) for exactly what has and has not been
> tested.

## Repository layout

```
firmware/
  loco/     PlatformIO project — receiver / drive controller  (rev D)
  remote/   PlatformIO project — handheld transmitter          (rev F)
hardware/
  wiring-loco.svg / .png     wiring diagram, locomotive (SVG is the source)
  wiring-remote.svg / .png   wiring diagram, remote
cad/
  Train.3dm / .stp             locomotive body + chassis
  Remote.3dm / .stp            remote enclosure
  pulley-and-wheels.3dm / .stp drive pulley and wheels
stl/
  9 print-ready meshes, generated from the STEP files above
docs/
  bom.md            bill of materials
  wiring-loco.md    wire list L1–L29, measurements, DRV8833 module notes
  wiring-remote.md  wire list W1–W19, battery sensing, deep sleep
  assembly.md       print settings and mechanical assembly
  status.md         build log, what is tested, what is next
```

Only the current revisions are in this repository (loco rev D, remote rev F). Earlier revisions
targeted the XIAO ESP32-S3, which has a different pinout; do not mix them.

## Quick start

### 1. Flash the firmware

Both projects are PlatformIO projects. The ESP32-C5 needs arduino-esp32 core ≥ 3.3, which is not in
the official `espressif32` platform, so both `platformio.ini` files point at the
[pioarduino](https://github.com/pioarduino/platform-espressif32) fork (release 55.03.311). That
requires **PlatformIO Core ≥ 6.1.19**.

```bash
cd firmware/remote        # or firmware/loco
pio run -t upload
pio device monitor -b 115200
```

On Windows, set `PYTHONIOENCODING=utf-8` (or run `chcp 65001`) first — otherwise the upload aborts
halfway with a `UnicodeEncodeError` when esptool draws its progress bar.

No pairing is needed: both ends find each other through a shared `MAGIC` key on Wi-Fi channel 1.

**Flash and bench-test before you solder anything.** With a bare XIAO you can already verify the
radio link, the headlight and the horn.

### 2. Print the parts

Nine parts, ready to slice, in [`stl/`](stl/) — see [`stl/README.md`](stl/README.md) for the part
list and quantities, and [`docs/assembly.md`](docs/assembly.md) for orientation, supports and print
settings. PETG, 0.4 nozzle, 0.2 mm layers, 3 perimeters, 20 % infill.

The STLs are derived from the STEP files in [`cad/`](cad/); the CAD is the source of truth.

### 3. Order the parts

See [`docs/bom.md`](docs/bom.md). Two things in there are easy to get wrong:

- the buck converter must be an **adjustable** MP1584EN set to **5.0 V** (not a fixed Mini-560);
- the master switch in the locomotive carries the full motor current — use one rated **≥ 2 A**, plus
  a 2 A fuse.

### 4. Wire it up

Follow [`docs/wiring-loco.md`](docs/wiring-loco.md) and [`docs/wiring-remote.md`](docs/wiring-remote.md)
alongside the diagrams in [`hardware/`](hardware/). Both documents give the assembly order, the
verification measurements to take before fitting the cells, and the expected values after switching
on. The locomotive has **two separate grounds** (PWR and SIG) joined only through the buck — that is
not a detail, it is what keeps 1 A of motor current out of the ADC reference.

## How it behaves

- Joystick forward/back = speed, press = horn.
- **Neutral lockout**: after power-up or signal loss the locomotive only drives once the stick has
  been at centre.
- **Failsafe**: the locomotive stops 400 ms after signal loss.
- Green LED on the remote = link alive (telemetry ack received from the locomotive).
- Red LED solid = remote cell low; blinking = **locomotive** pack low (the locomotive also stops and
  blinks its headlight).
- The remote deep-sleeps after 5 minutes idle; press the joystick to wake it. It does not sleep while
  a USB host was present at boot, so the COM port stays available while developing.
- Both units print a calibration line over USB every 2 seconds, so the battery measurement can be
  trimmed against a multimeter instead of guessed (`VDIV_CAL`).

## Known limitations

- The throttle scaling is symmetric while the joystick is not, so forward and reverse are not
  perfectly matched. Fix is described in [`docs/wiring-remote.md`](docs/wiring-remote.md).
- There is no breakaway (`MIN_DUTY`) threshold yet, so the first part of the stick travel produces
  whine rather than movement.
- `MAX_DUTY` is a percentage of pack voltage, so speed drops as the pack drains. The locomotive
  already measures the pack, so this can be compensated in firmware.
- Motor speed cannot be measured with the current hardware — no encoder, and the only header ADC pin
  is taken by the pack voltage divider.
- nSLEEP and nFAULT of the DRV8833 are deliberately not wired; protection is entirely in software.

See [`docs/status.md`](docs/status.md) for the full analysis and what is planned next.

## Safety

Lithium-ion cells, a balance charger and a motor driver in a toy: build it carefully.

- The 2S charger must be an IP2326-type **balance** board wired B−/BM/B+. Never charge this pack
  through a plain 1S board.
- Check cell polarity before fitting. The printed bays accept a reversed cell; the protection inside
  the cell will not save the charger.
- The low-battery auto-stop is the primary over-discharge guard — do not disable it.
- Master switch off while charging and while flashing, on both units.
- Body screws on before it goes near a child; charging only under supervision.

## Licence

Firmware is MIT; hardware, CAD and documentation are CERN-OHL-S v2. See [`LICENSE`](LICENSE).

BRIO is a trademark of BRIO AB. This is an independent hobby project, not affiliated with or endorsed
by BRIO.
