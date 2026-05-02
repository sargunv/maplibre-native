# render-tests/icon-text-fit/both-text-anchor-1x-image-2x-screen

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/icon-text-fit/both-text-anchor-1x-image-2x-screen/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `symbol,circle`
- Camera: none
- Diff stats: 12568 px, 10.4733%, bbox `(48,26)-(351,273)`

## Observation

Layout matches expected, but faint diffs cover icon/text edges and anchor dots across the full anchor grid.

## Likely Issue Class

1x sprite upscaling on a 2x screen, texture filtering, or fractional fit rounding.

## Evidence

The same family as the 2x-image case fails more broadly when the source sprite pixel ratio differs from the screen pixel ratio.

## Suggested Next Probe

Compare sprite atlas pixel-ratio handling and sampler coordinates between 1x and 2x fitted icon paths.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
