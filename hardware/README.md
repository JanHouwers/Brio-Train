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

**This is automated.** Push a changed SVG and the
[`Render wiring diagrams`](../.github/workflows/render-diagrams.yml) workflow re-renders both PNGs
and commits them back. Pull requests do not get a commit; there the workflow fails if the committed
PNGs no longer match their SVG, so a stale image cannot be merged.

To do it by hand — after editing an SVG locally, or to check the result before pushing:

```powershell
pwsh tools/render-diagrams.ps1
# or, with Windows PowerShell:
powershell -ExecutionPolicy Bypass -File tools\render-diagrams.ps1
```

The script finds Chrome or Edge, reads each SVG's own `width`/`height`, renders at 2× that size and
verifies the resulting image really has those dimensions.

**Render on Windows.** The diagrams specify `Segoe UI, Arial, sans-serif` and were laid out against
Segoe UI metrics; a Linux renderer falls back to a different font and text starts to overflow its
boxes. That is also why the workflow runs on `windows-latest`.
