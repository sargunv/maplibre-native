# render-tests/projection/axonometric

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/projection/axonometric/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill-extrusion`
- Camera: `zoom=18`
- Diff stats: 6327 px, 2.4136%, bbox `(107,32)-(404,367)`

## Observation

Actual and expected are close, but diff shows vertical face brightness or segment differences on isolated extrusions.

## Likely Issue Class

Axonometric projection face classification, lighting, or minor geometry precision.

## Evidence

Single fill-extrusion layer with no obvious interlayer ordering; diff is face-local.

## Suggested Next Probe

Compare generated side-face normals, winding, and lighting inputs under axonometric mode.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
