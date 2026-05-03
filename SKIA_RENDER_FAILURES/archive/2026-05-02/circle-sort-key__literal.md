> Archived 2026-05-02 after pending commit fixed Skia circle sort-key draw ordering.

# render-tests/circle-sort-key/literal

- Status: archived
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/circle-sort-key/literal/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `circle`
- Camera: `zoom=1`
- Diff stats: 582 px, 14.209%, bbox `(9,0)-(51,63)`

## Observation

Overlapping red, blue, and green circles are composited in a different order.

## Likely Issue Class

`circle-sort-key` order is reversed or not applied consistently in Skia drawing.

## Evidence

Style uses `circle-sort-key: ["get", "sort-key"]`; expected shows more green visible, while actual hides green behind red/blue.

## Suggested Next Probe

Check the circle sort comparator/order before upload and draw in the Skia path.

## Work Log

- 2026-05-02: Passed focused filter and full Skia sweep after sorted circle features were emitted into separate draw-priority-ordered drawables.
- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Archived after a focused pass and full sweep pass.
