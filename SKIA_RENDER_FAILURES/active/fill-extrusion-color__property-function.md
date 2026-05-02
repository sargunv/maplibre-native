# render-tests/fill-extrusion-color/property-function

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-color/property-function/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill-extrusion`
- Camera: `zoom=18 pitch=60`
- Diff stats: 4200 px, 3.2043%, bbox `(57,73)-(342,255)`

## Observation

Actual has extra green/blue face strips over adjacent extrusions; expected occludes them.

## Likely Issue Class

In-layer extrusion feature ordering and depth semantics.

## Evidence

Property-driven red/green/blue values appear correct; mismatch is visible overdraw on overlapping faces.

## Suggested Next Probe

Probe per-feature draw order and depth writes for one fill-extrusion layer.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
