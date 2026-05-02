# render-tests/tile-lod/default

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/default/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=72 bearing=30`
- Diff stats: 56905 px, 21.7075%, bbox `(0,8)-(511,511)`

## Observation

Visible LOD bands match, but diff traces glyph and grid edges throughout with a subtle content shift.

## Likely Issue Class

Skia raster sampling or projection rounding mismatch.

## Evidence

The artifact is mostly edge-shaped, with no obvious wrong tile zoom level.

## Suggested Next Probe

Compare tile quad vertex positions and sampler/filter state against the reference backend.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
