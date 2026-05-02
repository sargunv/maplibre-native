# render-tests/text-variable-anchor/all-anchors-tile-map-mode

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/text-variable-anchor/all-anchors-tile-map-mode/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,symbol`
- Camera: `zoom=14`
- Diff stats: 66651 px, 6.3563%, bbox `(0,0)-(1023,1023)`

## Observation

Text labels mostly match. Expected includes a red tile grid that actual does not render.

## Likely Issue Class

Missing tile debug overlay, with possible minor residual text antialiasing.

## Evidence

Style uses `debug: true` and `mapMode: "tile"`; inspection found expected has red grid pixels absent from actual.

## Suggested Next Probe

Fix or confirm tile-boundary debug rendering first, then re-evaluate residual text placement.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
