# render-tests/tile-lod/distance-based-pitch-threshold

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/distance-based-pitch-threshold/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=61 bearing=30`
- Diff stats: 47280 px, 18.0359%, bbox `(0,0)-(511,511)`

## Observation

Effectively the same as `pitch-threshold`: matching visible zoom level with edge/subpixel differences.

## Likely Issue Class

Raster reprojection or subpixel transform mismatch.

## Evidence

Stats match the non-distance pitch-threshold case exactly.

## Suggested Next Probe

Confirm distance mode does not alter selection below threshold, then compare transform matrices against the non-distance case.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
