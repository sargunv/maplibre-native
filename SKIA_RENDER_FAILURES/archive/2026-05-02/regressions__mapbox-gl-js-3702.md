> Archived 2026-05-02 after focused and full sweeps passed with solid-fill near-plane clipping and tile-clip near-plane guard.

# render-tests/regressions/mapbox-gl-js#3702

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/regressions/mapbox-gl-js#3702/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill`
- Camera: `zoom=4 pitch=60`
- Diff stats: 7154 px, 43.6646%, bbox `(0,30)-(127,95)`

## Observation

Expected shows overlapping blue and red pitched polygons; actual is transparent/empty.

## Likely Issue Class

Same pitched GeoJSON fill omission/culling path as `#3320`, possibly triggered more strongly by high pitch.

## Evidence

The style uses two GeoJSON fill features with identity `fill-color`; actual has only transparent pixels.

## Suggested Next Probe

Compare bucket/vertex counts before draw against the reference path, then lower pitch to find the culling threshold.

## Work Log

- 2026-05-02: Created from full Skia sweep and miscellaneous inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
