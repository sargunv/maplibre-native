# render-tests/heatmap-weight/identity-property-function

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/heatmap-weight/identity-property-function/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,heatmap`
- Camera: `zoom=14`
- Diff stats: 22945 px, 70.0226%, bbox `(0,0)-(255,127)`

## Observation

Image is nearly identical, but subtle global gradient/color differences are visible around contour edges.

## Likely Issue Class

Heatmap accumulation, color-ramp precision, or sampling difference.

## Evidence

Identity weight uses property `localrank`; alpha is unchanged and differences are low magnitude despite broad pixel count.

## Suggested Next Probe

Compare heatmap weight accumulation values before colorization between Skia and the reference path.

## Work Log

- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
