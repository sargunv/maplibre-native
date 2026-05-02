# render-tests/combinations/color-relief-translucent--hillshade-translucent-low-zoom

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/combinations/color-relief-translucent--hillshade-translucent-low-zoom/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,color-relief,hillshade`
- Camera: `zoom=1`
- Diff stats: 47656 px, 72.7173%, bbox `(0,0)-(255,255)`

## Observation

Overall image is close; diff is mostly small specks and ridge/coast differences over terrain.

## Likely Issue Class

Low-zoom DEM sampling and hillshade precision.

## Evidence

Many pixels differ, but mean absolute RGB deltas are small and alpha is unchanged.

## Suggested Next Probe

Compare hillshade normal/elevation samples around ridge/coast pixels and check low-zoom exaggeration scaling.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
