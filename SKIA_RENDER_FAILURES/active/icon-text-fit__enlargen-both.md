# render-tests/icon-text-fit/enlargen-both

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/icon-text-fit/enlargen-both/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `symbol`
- Camera: none
- Diff stats: 398 px, 4.8584%, bbox `(5,18)-(122,46)`

## Observation

The fitted orange/blue box appears aligned, but diff shows vertical edge strips and slight glyph antialiasing.

## Likely Issue Class

Fitted icon extent rounding or edge sampling for an enlarged `small-box` sprite.

## Evidence

Single-symbol case with `text-field: "ABCD efgh"` and `icon-text-fit: both`; changed pixels are confined to the fitted icon/glyph bounds.

## Suggested Next Probe

Inspect computed fitted box width/height and final device-pixel rounding for `icon-text-fit: both`.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
