# render-tests/icon-text-fit/both-text-anchor-2x-image-2x-screen

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/icon-text-fit/both-text-anchor-2x-image-2x-screen/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `symbol,circle`
- Camera: none
- Diff stats: 4024 px, 3.3533%, bbox `(52,29)-(350,272)`

## Observation

Actual and expected are visually close. The diff concentrates around fitted icon right edges and green anchor circles.

## Likely Issue Class

Icon-text-fit quad rounding or sprite sampling at `pixelRatio: 2`.

## Evidence

The style uses a 2x screen, 2x sprite, `icon-text-fit: both`, and multiple `text-anchor` values. Actual has slightly more alpha-only coverage than expected.

## Suggested Next Probe

Log fitted icon quad dimensions and anchor offsets before Skia rasterization for each `text-anchor`.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
