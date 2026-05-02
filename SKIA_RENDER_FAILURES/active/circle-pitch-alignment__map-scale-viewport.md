# render-tests/circle-pitch-alignment/map-scale-viewport

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/circle-pitch-alignment/map-scale-viewport/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `circle`
- Camera: `zoom=0 pitch=60`
- Diff stats: 734 px, 2.24%, bbox `(24,70)-(103,227)`

## Observation

The same three blue ellipses render, but edge antialiasing and coverage differ.

## Likely Issue Class

Skia ellipse rasterization or projected circle coverage tolerance.

## Evidence

Actual and expected bboxes match; diff is mostly outline-only.

## Suggested Next Probe

Compare generated ellipse geometry and antialiasing mode; geometry is likely broadly correct.

## Work Log

- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
