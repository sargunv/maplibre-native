# render-tests/heatmap-radius/literal

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/heatmap-radius/literal/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,heatmap`
- Camera: `zoom=14`
- Diff stats: 21405 px, 65.3229%, bbox `(0,0)-(255,127)`

## Observation

Contours and high-intensity boundaries are slightly shifted or brighter in actual.

## Likely Issue Class

Heatmap kernel radius sampling or texture filtering precision.

## Evidence

Style uses literal `heatmap-radius: 20`; expected image is closer than the half-float expected variant.

## Suggested Next Probe

Verify radius-to-kernel-size conversion and sampling coordinates in Skia heatmap rendering.

## Work Log

- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
