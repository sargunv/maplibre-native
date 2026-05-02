# render-tests/tile-lod/pitch-threshold

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/pitch-threshold/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=61 bearing=30`
- Diff stats: 47280 px, 18.0359%, bbox `(0,0)-(511,511)`

## Observation

The same tile zoom level appears selected, but glyph/grid edges are shifted or antialiased differently across the image.

## Likely Issue Class

Raster reprojection, subpixel transform, or Skia sampling mismatch rather than LOD choice.

## Evidence

Style sets `setTileLodPitchThreshold 62` at pitch `61`; diff is edge-shaped.

## Suggested Next Probe

Log selected tile z/x/y and final quad matrices at pitch `61`, `62`, and `63`.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
