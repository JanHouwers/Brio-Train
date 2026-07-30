# Wiring diagrams

| Unit | Source | Rendered |
|------|--------|----------|
| Locomotive, rev D | `wiring-loco.svg` | `wiring-loco.png` (3580 × 2580) |
| Remote, rev F | `wiring-remote.svg` | `wiring-remote.png` (2800 × 2040) |

**The SVG is the source.** The PNGs are rendered from it at 2× scale for viewing and printing; if you
change a diagram, edit the SVG and re-render.

Read them alongside the wire lists — [`../docs/wiring-loco.md`](../docs/wiring-loco.md) and
[`../docs/wiring-remote.md`](../docs/wiring-remote.md) — which carry the wire numbers, gauges,
assembly order and verification measurements.

Conventions used in both diagrams: a filled dot is a connection, a crossing without a dot is not.
In the locomotive diagram a thick line carries motor current (AWG20). Wire numbers (L1…L31, W1…W19)
match the wire lists exactly.

## Re-rendering

Any SVG renderer will do. With Chrome or Edge headless:

```bash
chrome --headless --disable-gpu --hide-scrollbars \
       --default-background-color=ffffffff --force-device-scale-factor=2 \
       --window-size=1790,1290 --screenshot=wiring-loco.png \
       file:///absolute/path/to/wiring-loco.svg
```

The `--window-size` must match the SVG's own `width`/`height` (1790 × 1290 for the locomotive,
1400 × 1020 for the remote); the scale factor then doubles the output resolution.
