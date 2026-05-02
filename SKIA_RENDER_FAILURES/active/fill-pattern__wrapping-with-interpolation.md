# render-tests/fill-pattern/wrapping-with-interpolation

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-pattern/wrapping-with-interpolation/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,fill`
- Camera: `zoom=0.99`
- Diff stats: 6581 px, 2.5105%, bbox `(0,0)-(511,511)`

## Observation

Black/white land-water mask mostly matches, with coastline and wrapping-boundary discrepancies.

## Likely Issue Class

Tile wrapping, interpolation, or source replacement edge coverage.

## Evidence

Style uses `zoom: 0.99`, `center: [0.7,0]`, and source replacement to MLT; diff is coastline-shaped.

## Suggested Next Probe

Compare MVT and MLT output directly, then test `zoom: 1` and nearby centers to see whether wrap interpolation crosses tile/world boundaries incorrectly.

## Work Log

- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
