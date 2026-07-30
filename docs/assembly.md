# Printing and assembly

> **Status note.** The mechanical instructions below were written for the earlier OpenSCAD/FreeCAD
> version of the parts. The mechanics (drive concept, axles, magnets, screw positions) are unchanged,
> but the current geometry lives in the Rhino/STEP files in [`../cad/`](../cad/), and the body is now
> split into a chassis plus three glued top pieces instead of one part. Print-ready meshes are in
> [`../stl/`](../stl/) — check the details below against that geometry before printing. The
> electrical side is current: see [`wiring-loco.md`](wiring-loco.md) and
> [`wiring-remote.md`](wiring-remote.md).

## Drive concept — read before printing

The N20 is 24 mm long; the space between the wheels is only 21 mm, so the motor **cannot** sit on the
axle. It sits in a tunnel **above** the rear wheels and drives the rear-right wheel 1:1 through an
18×2 O-ring belt (pulley on the motor shaft → grooved hub on the wheel). Belt tension pulls the motor
into its seat. One driven wheel with a rubber tyre (the second O-ring) is enough — the battery weight
sits on top of it.

Speed: 150 rpm × Ø21 effective ≈ **16 cm/s**, capped further in firmware via `MAX_DUTY`.

## Print settings

PETG, 0.4 nozzle, 0.2 mm layers, 3 perimeters, 20 % infill.

| File in [`../stl/`](../stl/) | Qty | Orientation | Notes |
|------|-----|-------------|-------|
| `loco-chassis.stl` | 1 | as-is (plate down) | supports under the motor box + coupler arms |
| `loco-boiler.stl` | 1 | as-is | supports in the wheel arches |
| `loco-cab.stl` | 1 | as-is | supports at the window tops |
| `loco-front.stl` | 1 | as-is | short part, prints flat against the plate |
| `loco-wheel-plain.stl` | 3 | as-is | 2× front (press onto the Ø3 rod), 1× rear left with an M3 washer against the boss |
| `loco-wheel-driven.stl` | 1 | as-is (outer face down) | rear right, belt hub inward |
| `loco-pulley.stl` | 1 | as-is | D-bore, push onto the N20 shaft |
| `remote-bottom.stl` / `remote-top.stl` | 1+1 | flat face down (top printed upside-down) | no supports needed; the small board-retention lips are short flat overhangs and print fine |

Wall count ≥ 3 for the wheels — the hub takes the drive load.

## Assembly — locomotive

1. **Chassis**: press the AA spring/plate contacts into the battery-bay end slots (springs at one
   end, plates at the other; the two cells sit antiparallel — jumper the far-end contacts to make the
   series link, and take the middle tap from that jumper for the balance wire).
2. **Motor**: push the pulley onto the N20 D-shaft, slide the motor into the tunnel from the left
   side, shaft through the hole in the right end wall. Seat the pulley as close to the end wall as
   possible. A dab of hot glue at the tunnel mouth.
3. **Belt**: loop one 18×2 O-ring from the pulley down through the belt window onto the driven-wheel
   hub groove, then screw the driven wheel (M3×12 + washer) into the right stub pilot. Left wheel:
   M3×12 + washer. Stretch the second O-ring into the tread groove first.
4. **Front axle**: push the Ø3×32 rod through the chassis bore, press a wheel onto each end
   (0.1–0.2 mm clearance under the rod so wheels and rod spin together).
5. **Electronics**: 2S charger flat against the inside of the body rear wall — the USB-C opening sits
   slightly left of centre (the board occupies the left ~two-thirds of the wall). The power switch
   stands **vertically** in the slot near the right rear corner. Buck + DRV8833 + XIAO sit on top of
   the battery bay. Wire per [`wiring-loco.md`](wiring-loco.md) — **firmware first, wiring second**,
   and follow the order of assembly in that document.

   > The separate e-shelf tray of the earlier design is not part of the current CAD; check how the
   > boards are retained in `Train.3dm` before wiring them in.
6. **Magnets**: check the polarity against a real Brio wagon **before** gluing the Ø10×3 magnets into
   the coupler pockets (epoxy).
7. **Body**: the body is three printed pieces on top of the chassis — front, boiler and cab. The
   split exists so that each piece can be printed without much support; **glue the three together
   with superglue (cyanoacrylate)**. Dry-fit them first, and glue with the parts held against the
   chassis so the joints line up with it. The assembled body then drops over the chassis and is
   secured with 2× M3×12 from below into the internal pillars. Both screws sit off-centre: front at
   y = +10, rear at y = −12 (diagonally opposed), keeping the rear wall free for the charger board.
   The cab front wall has a notch that fits over the motor box.

## Assembly — remote

Both boards sit in rigid pockets — the case wall takes the USB-C plug forces (the plug overmould
bottoms out on the wall, a backstop takes the insertion force), and the boards are held down by lips
plus pillars in the top shell.

1. Press the 26650 spring/plate contacts into the end-wall slots, drop the cell into the cradle.
2. **TP4056** (long axis across the case, USB-C at the side wall): slip its rear edge under the lip
   on the pocket backstop with the USB end tilted up, then lower the USB end until the connector sits
   in the side-wall opening. Route the B+/B− wires through the notch in the backstop or over it.
3. **XIAO**: solder the BAT wires to the underside pads first. Tilt the board nose-down, slide the
   USB-C end under the two lips at the end wall, drop the rear onto the ledges. The wires run in the
   channel under the board and out through the backstop notch. Stick a 1–2 mm foam pad on top of the
   RF shield.
4. Joystick screwed to the 4 posts under the top-shell hole, LEDs into the top holes, switch into its
   slot.
5. Close the shells: the top-shell pillars press the XIAO (via the foam pad) and the TP4056 front
   edges down. 4× M3×12 from the top.

## Before printing — verify against your own set

1. **Magnet height**: measure the magnet centre height of one of your wagons above the rail surface;
   the design assumes 13 mm above track top.
2. **Tightest curve**: if the locomotive binds on small-radius curves, increase the wheel chamfer to
   2 mm.
3. **Cell length**: protected 14500 cells are 50.5 mm, 0.5 mm longer than a standard AA. Check the
   bay before printing the full chassis.

## Safety notes

- The 2S charger must be an IP2326-type **balance** board wired B−/BM/B+. Never charge the pack
  through a plain 1S board.
- Cells are protected, but the low-battery auto-stop (6.6 V trip / 6.9 V clear) is the primary
  over-discharge guard — do not disable it.
- Body screws on: no child access to the cells. Charging only under supervision.
- Master switch off while charging and while flashing, on both units.
