# render-tests/line-dasharray/zero-length-gap

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/line-dasharray/zero-length-gap/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `line`
- Camera: `zoom=0`
- Diff stats: 3441 px, 20.3658%, bbox `(7,1)-(120,125)`

## Observation

Actual and expected visually nearly match; diffs are thin horizontal antialiasing changes around dashed rows.

## Likely Issue Class

Dash normalization or zero-length dash/gap edge antialiasing.

## Evidence

The style exercises dasharrays `[1,0,1,1]`, `[1,1,0]`, `[1,0,1]`, `[1,0]`, and `[0,1]`; solid color counts mostly match.

## Suggested Next Probe

Probe each dasharray in isolation and compare row-by-row alpha masks, especially `[0,1]` and odd-length arrays after normalization.

## Work Log

- 2026-05-02: Tested adding a Skia texel-center `+0.5` offset to line SDF atlas sampling. Focused run still failed, so the issue is not the same normalized-to-pixel sampling offset as line-gradient ramps.
- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
