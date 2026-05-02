# render-tests/fill-extrusion-base/negative

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-base/negative/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill,fill-extrusion`
- Camera: `zoom=18 pitch=60`
- Diff stats: 60534 px, 23.0919%, bbox `(75,90)-(405,346)`

## Observation

Actual shows a small raised gray/white cube, while expected has a larger dark extrusion mass with different base placement.

## Likely Issue Class

Negative `fill-extrusion-base` geometry semantics or base/height clamping.

## Evidence

Style includes negative base values; this looks more like geometry construction than simple depth ordering.

## Suggested Next Probe

Probe base/height normalization for negative base values in Skia extrusion tessellation.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
