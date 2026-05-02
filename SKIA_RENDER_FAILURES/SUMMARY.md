# Skia Render Failure Summary

Baseline: 2026-05-02 full Skia sweep, `53 failed`.

## Current Clusters

- Missing debug tile overlays: `map-mode/tile-avoid-edges`, `symbol-placement/*tile-map-mode`, `text-variable-anchor/*tile-map-mode` mostly differ because expected has red tile boundaries that Skia does not render.
- Fill-extrusion depth/order: most `fill-extrusion-*` and `projection/axonometric-multiple` failures show occlusion, depth-buffer, or inter-layer 2D/3D ordering differences.
- Pitched fill omission: `regressions/mapbox-gl-js#3320` and `#3702` render expected pitched fills but Skia actual is empty.
- Terrain/color-relief: simple low-zoom terrain is a low-amplitude DEM precision issue, while multi-DEM ocean/bathymetry combinations look like negative-elevation or transparent-stop handling.
- Raster tile LOD: most tile LOD failures preserve visible LOD bands but differ along projected raster edges; `distance-based-scale` additionally shows missing/black horizon coverage.
- Pattern under transforms: line/fill/background pattern failures concentrate around pitch, DPR, uneven sprites, and wrapping.
- Symbol/icon registration: icon-text-fit and pitched/rolled icons are mostly edge sampling, fitted-quad rounding, or subpixel transform differences.
- Heatmap precision: heatmap radius/weight failures are widespread but low-magnitude color-ramp or kernel sampling differences.
- Circle semantics: circle failures split into antimeridian wrapping, sort-key ordering, and pitched circle edge coverage.

## Highest-Leverage Probes

1. Implement or fix Skia debug tile boundary rendering for tile map mode; this may clear several symbol/map-mode failures without touching placement.
2. Isolate fill-extrusion depth-buffer semantics before chasing individual extrusion color/translate/projection cases.
3. Reproduce pitched fill omission with pitch toggled to find the projection/culling threshold.
4. Split pattern failures with solid-color equivalents to separate geometry/coverage from sprite sampling.
5. Inspect ocean DEM decode and negative color-relief stops before changing general terrain sampling.
