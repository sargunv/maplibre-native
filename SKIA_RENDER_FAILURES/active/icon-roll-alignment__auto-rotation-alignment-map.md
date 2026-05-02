# render-tests/icon-roll-alignment/auto-rotation-alignment-map

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/icon-roll-alignment/auto-rotation-alignment-map/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `symbol`
- Camera: `pitch=45 bearing=45 roll=45`
- Diff stats: 30 px, 0.7324%, bbox `(29,27)-(34,37)`

## Observation

The map-aligned icon differs by a few antialiased pixels, with the same overall bbox.

## Likely Issue Class

Subpixel transform or antialiasing difference with roll, pitch, bearing, and `icon-rotation-alignment: map`.

## Evidence

Only 30 pixels differ around the icon; this is the smallest active visual mismatch.

## Suggested Next Probe

Render a larger diagnostic icon and compare the final symbol transform matrix for roll plus map rotation alignment.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
