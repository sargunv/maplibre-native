# render-tests/fill-extrusion-translate/function

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-translate/function/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill,fill-extrusion`
- Camera: `zoom=19 pitch=60`
- Diff stats: 15749 px, 12.0155%, bbox `(190,0)-(414,202)`

## Observation

Cyan ground strip expected at right/bottom is mostly occluded in actual.

## Likely Issue Class

Zoom-function translate value appears evaluated, but extrusion occlusion footprint is wrong.

## Evidence

Style uses exponential translate stops; actual extrusion placement roughly matches while fill visibility does not.

## Suggested Next Probe

Log the evaluated translate vector and compare projected side/top polygons before depth sorting.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
