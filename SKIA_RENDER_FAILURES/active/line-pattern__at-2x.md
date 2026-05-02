# render-tests/line-pattern/@2x

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/line-pattern/@2x/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `line`
- Camera: none
- Diff stats: 1051 px, 6.4148%, bbox `(35,26)-(91,95)`

## Observation

Actual and expected are close; differences are small edge/alpha shifts around the three patterned horizontal lines.

## Likely Issue Class

HiDPI sprite/pattern scaling or texture-coordinate rounding.

## Evidence

Style sets `pixelRatio: 2` with line widths `2`, `4`, and `8`; color counts are close, with edge pixels differing.

## Suggested Next Probe

Inspect sprite pixel-ratio handling and line-pattern atlas UV scaling at device pixel ratio 2.

## Work Log

- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
