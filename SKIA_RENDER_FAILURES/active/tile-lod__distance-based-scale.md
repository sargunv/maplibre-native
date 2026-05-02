# render-tests/tile-lod/distance-based-scale

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/tile-lod/distance-based-scale/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `raster`
- Camera: `zoom=16 pitch=72 bearing=30`
- Diff stats: 83907 px, 32.008%, bbox `(0,0)-(511,511)`

## Observation

Actual has a large black missing/empty wedge near the upper-right horizon and shifted LOD bands.

## Likely Issue Class

Distance-based LOD coverage or clipping bug, especially with scale override.

## Evidence

This is the strongest tile LOD miss and includes alpha/coverage differences, unlike mostly edge-only LOD cases.

## Suggested Next Probe

Inspect distance-mode tile cover radius and frustum clipping with scale `3`; log rejected far tiles.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain/raster inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
