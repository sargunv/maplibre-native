# render-tests/tile-lod/min-radius

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/min-radius/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=72 bearing=30`
- Diff stats: 60322 px, 23.011%, bbox `(0,8)-(511,511)`

## Observation

Same general image and LOD bands as expected, with edge differences and slight boundary shift.

## Likely Issue Class

Tile cover radius or projection rounding.

## Evidence

Style sets `setTileLodMinRadius 1`; diff is mostly edge-heavy rather than a completely different tile selection.

## Suggested Next Probe

Log the min-radius-expanded tile set and compare with default; verify selected tiles are identical where expected.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
