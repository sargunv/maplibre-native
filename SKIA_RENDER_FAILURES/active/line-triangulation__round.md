# render-tests/line-triangulation/round

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/line-triangulation/round/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,line`
- Camera: `zoom=14`
- Diff stats: 25253 px, 9.6333%, bbox `(0,180)-(511,389)`

## Observation

Geometry mostly matches, but actual line is much darker and more desaturated than expected.

## Likely Issue Class

Color/opacity blending or premultiplied-alpha handling, not primarily triangulation shape.

## Evidence

Style uses `line-opacity: 0.5`; expected line color over white is much lighter than actual.

## Suggested Next Probe

Render the same geometry with `line-opacity: 1`, then with miter/bevel joins, to separate opacity/blending from round-join tessellation.

## Work Log

- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
