# Print-ready STLs

Meshed from the STEP files in [`../cad/`](../cad/), which are the Rhino exports of the current
design. Every part is translated to sit on **Z = 0** and centred in X/Y; the orientation is the one
from the CAD.

| File | Qty | Size (mm) | Facets | From |
|------|-----|-----------|--------|------|
| `loco-chassis.stl` | 1 | 130.0 × 34.8 × 35.0 | 4,450 | `Train.stp` → `chassis` |
| `loco-boiler.stl` | 1 | 68.0 × 40.1 × 52.0 | 19,266 | `Train.stp` → `Body` |
| `loco-cab.stl` | 1 | 42.0 × 43.0 × 46.0 | 7,962 | `Train.stp` → `Body001` |
| `loco-front.stl` | 1 | 8.9 × 40.1 × 45.0 | 6,386 | `Train.stp` → `Voorkant` |
| `loco-wheel-plain.stl` | 3 | Ø20.8 × 5.0 | 3,292 | `pulley-and-wheels.stp` → `#1305` |
| `loco-wheel-driven.stl` | 1 | Ø20.0 × 7.6 | 2,584 | `pulley-and-wheels.stp` → `Pulley001` |
| `loco-pulley.stl` | 1 | Ø11.0 × 4.0 | 1,120 | `pulley-and-wheels.stp` → `Pulley` |
| `remote-bottom.stl` | 1 | 170.0 × 56.0 × 22.0 | 6,410 | `Remote.stp` → `Remote bottom` |
| `remote-top.stl` | 1 | 170.0 × 56.0 × 29.2 | 25,984 | `Remote.stp` → `Remote top` |

The locomotive body is split into four printable pieces — chassis, boiler, cab and front — instead of
the single body of earlier revisions.

**Which wheel is which:** `loco-wheel-driven` is the thicker one (7.6 mm) and carries the belt hub;
`loco-wheel-plain` (5.0 mm) is the running wheel, used twice at the front and once at the rear left.
If your build disagrees, check the two files against the CAD before printing four wheels.

## Mesh quality

Tessellated at 0.03 mm linear deviation and 0.2 rad angular deviation. All nine meshes are closed
(watertight), have no self-intersections, and their volumes are within 0.32 % of the CAD solids.

## Regenerating

The STLs are derived files — the CAD in [`../cad/`](../cad/) is the source. Export from Rhino, or
regenerate from the STEP files with FreeCAD:

```python
import Import, Mesh, MeshPart
from FreeCAD import Vector
doc = FreeCAD.newDocument("exp")
Import.insert("cad/Train.stp", doc.Name)
obj = doc.getObjectsByLabel("chassis")[0]
sh = obj.Shape.copy()
bb = sh.BoundBox
sh.translate(Vector(-(bb.XMin + bb.XMax) / 2, -(bb.YMin + bb.YMax) / 2, -bb.ZMin))
m = doc.addObject("Mesh::Feature", "m")
m.Mesh = MeshPart.meshFromShape(Shape=sh, LinearDeflection=0.03,
                                AngularDeflection=0.2, Relative=False)
Mesh.export([m], "stl/loco-chassis.stl")
```

See [`../docs/assembly.md`](../docs/assembly.md) for print settings, orientation and supports.
