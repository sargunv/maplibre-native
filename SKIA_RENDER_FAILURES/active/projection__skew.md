# render-tests/projection/skew

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/projection/skew/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill-extrusion`
- Camera: `zoom=18`
- Diff stats: 13939 px, 5.3173%, bbox `(0,123)-(404,511)`

## Observation

Actual has extra diagonal red/black side slivers and shifted face geometry; expected prisms are cleaner.

## Likely Issue Class

Projection/skew transform or extrusion side-face geometry mismatch.

## Evidence

Style uses `metadata.test.skew: [-1, -1]`; diff follows projected side faces rather than only inter-layer overlaps.

## Suggested Next Probe

Inspect skew matrix application to top and side vertices separately.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
