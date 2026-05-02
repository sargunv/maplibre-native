# render-tests/heatmap-radius/function

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/heatmap-radius/function/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,heatmap`
- Camera: `zoom=14`
- Diff stats: 19973 px, 60.9528%, bbox `(0,0)-(255,127)`

## Observation

Subtle contour edge differences match the literal radius case.

## Likely Issue Class

Heatmap radius evaluation or sampling precision, probably not a gross zoom-function value error.

## Evidence

The broad shape matches expected, but low-magnitude color differences remain across contours.

## Suggested Next Probe

Log evaluated radius and compare generated heatmap kernel dimensions against the non-Skia path.

## Work Log

- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
