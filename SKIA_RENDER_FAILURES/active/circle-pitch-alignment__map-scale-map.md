# render-tests/circle-pitch-alignment/map-scale-map

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/circle-pitch-alignment/map-scale-map/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `circle`
- Camera: `zoom=0 pitch=60`
- Diff stats: 1007 px, 3.0731%, bbox `(10,73)-(117,237)`

## Observation

The same three blue ellipses render, but outline coverage differs; the bottom ellipse is about one pixel wider in actual.

## Likely Issue Class

Projected circle geometry rounding plus Skia antialiasing.

## Evidence

Actual bbox is slightly wider than expected, while the feature positions match broadly.

## Suggested Next Probe

Inspect map-scale circle radius projection and final vertex rounding before Skia draw.

## Work Log

- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
