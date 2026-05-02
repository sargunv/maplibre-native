# render-tests/tile-lod/scale

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/scale/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=72 bearing=30`
- Diff stats: 64119 px, 24.4595%, bbox `(0,8)-(511,511)`

## Observation

Broad edge differences and slight perspective/content shift; top horizon has coverage artifacts.

## Likely Issue Class

Tile LOD scale/frustum coverage plus raster sampling mismatch.

## Evidence

Style sets `setTileLodScale 3`; changed alpha appears near the horizon.

## Suggested Next Probe

Check tile cover computation near the horizon for `setTileLodScale(3)` and verify no far-plane quads are missing.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
