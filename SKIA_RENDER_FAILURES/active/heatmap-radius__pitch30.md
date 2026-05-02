# render-tests/heatmap-radius/pitch30

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/heatmap-radius/pitch30/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,heatmap`
- Camera: `zoom=14 pitch=30`
- Diff stats: 21307 px, 65.0238%, bbox `(0,0)-(255,127)`

## Observation

Subtle contour shifts remain under pitch, especially near upper/right hot regions.

## Likely Issue Class

Heatmap kernel projection or sampling precision under pitch.

## Evidence

Same base pattern as literal radius plus `pitch: 30`; failure remains edge/gradient-level.

## Suggested Next Probe

Compare pitched heatmap screen-space coordinates and kernel scaling before rasterization.

## Work Log

- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
