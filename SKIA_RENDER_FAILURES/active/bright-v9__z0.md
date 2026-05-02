# render-tests/bright-v9/z0

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/bright-v9/z0/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: style import
- Camera: none
- Diff stats: 86028 px, 32.8171%, bbox `(0,0)-(511,447)`

## Observation

Actual has more or darker coastlines, island outlines, and boundary detail than expected; differences trace vector linework worldwide.

## Likely Issue Class

Baseline/style/source fixture mismatch or line rendering/filtering difference at z0.

## Evidence

Style loads `local://mapbox-gl-styles/styles/bright-v9.json`; diff follows coast/border geometry rather than a localized Skia artifact.

## Suggested Next Probe

Compare the local bright-v9 style and vector tile fixtures used to generate expected; then check z0 line filters and antialias settings.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
