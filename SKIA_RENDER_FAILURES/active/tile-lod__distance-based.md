# render-tests/tile-lod/distance-based

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/distance-based/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=72 bearing=30`
- Diff stats: 63290 px, 24.1432%, bbox `(0,7)-(511,511)`

## Observation

Distance-based LOD pattern appears correct; differences mostly outline glyph and grid edges.

## Likely Issue Class

Raster transform or sampling mismatch more than tile selection.

## Evidence

Style sets `setTileLodMode distance`; diff is edge-shaped with low signed channel bias.

## Suggested Next Probe

Log selected zoom per screen tile and compare with reference; if selection matches, focus on raster sampling.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
