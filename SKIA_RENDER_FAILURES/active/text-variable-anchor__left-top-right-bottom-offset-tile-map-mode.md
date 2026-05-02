# render-tests/text-variable-anchor/left-top-right-bottom-offset-tile-map-mode

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/text-variable-anchor/left-top-right-bottom-offset-tile-map-mode/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,symbol`
- Camera: `zoom=14`
- Diff stats: 67531 px, 6.4403%, bbox `(0,0)-(1023,1023)`

## Observation

Restaurant labels and icons largely match, but expected has a red tile grid missing from actual. Small speckles remain around some icons.

## Likely Issue Class

Missing tile debug overlay plus possible minor icon/text registration around variable-anchor radial offsets.

## Evidence

Style uses `text-radial-offset: 0.7` and variable anchors `left/top/right/bottom`; the largest diff component is the absent red grid.

## Suggested Next Probe

Isolate and fix debug grid rendering first. If still failing, compare selected variable-anchor/icon offsets for labels with residual speckle diffs.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
