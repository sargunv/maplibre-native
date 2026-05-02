# render-tests/combinations/color-relief--hillshade--vector-fill--color-relief--hillshade

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/combinations/color-relief--hillshade--vector-fill--color-relief--hillshade/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,color-relief,hillshade,fill`
- Camera: `zoom=1`
- Diff stats: 62257 px, 94.9966%, bbox `(0,0)-(255,255)`

## Observation

Same major ocean/bathymetry loss as the no-vector variant; vector land fill is not the differentiator.

## Likely Issue Class

Ocean DEM negative-elevation decode or compositing in multi-DEM color-relief/hillshade stacks.

## Evidence

The failure matches `color-relief--hillshade--color-relief--hillshade` with similar signed color bias.

## Suggested Next Probe

Render ocean DEM color-relief alone in Skia, then add hillshade/color-relief layers back one at a time.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
