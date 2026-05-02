# render-tests/color-relief/low-zoom

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/color-relief/low-zoom/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,color-relief`
- Camera: `zoom=1`
- Diff stats: 30133 px, 45.9793%, bbox `(0,0)-(255,255)`

## Observation

Actual and expected are visually close; diff is sparse along coast/ridge boundaries plus faint low-amplitude terrain texture.

## Likely Issue Class

DEM sampling or elevation interpolation precision at low zoom.

## Evidence

Many pixels differ, but mean channel delta is small and alpha is unchanged.

## Suggested Next Probe

Sample elevation values before color-ramp lookup at several diff-heavy pixels and compare low-zoom exaggeration scaling.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
