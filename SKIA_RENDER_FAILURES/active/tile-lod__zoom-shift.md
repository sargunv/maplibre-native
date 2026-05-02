# render-tests/tile-lod/zoom-shift

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/zoom-shift/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=72 bearing=30`
- Diff stats: 50982 px, 19.4481%, bbox `(0,8)-(511,511)`

## Observation

Zoom-shifted pattern with dense center tiles matches broadly; diff follows small-grid and glyph edges.

## Likely Issue Class

Raster reprojection, subpixel transform, or antialiasing mismatch.

## Evidence

Style sets `setTileLodZoomShift 2`; visible tile choice appears plausible.

## Suggested Next Probe

Verify zoom-shift tile IDs first; if matching, focus on Skia raster transform precision.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
