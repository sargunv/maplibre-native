# Skia Render Failure Summary

Baseline: 2026-05-02 full Skia sweep after Skia debug line rendering, `46 failed`.

## Resolved Clusters

- Tile-mode debug overlays now pass after Skia draws `DebugShader` line and line-strip drawables directly from `DebugUBO`. This archived `map-mode/tile-avoid-edges`, `symbol-placement/*tile-map-mode`, and `text-variable-anchor/*tile-map-mode` workpads.
- `color-relief/low-zoom` and `combinations/color-relief-translucent--hillshade-translucent-low-zoom` passed in the same full sweep and were archived as no-code-change passers.

## Current Clusters

- Fill-extrusion depth/order: most `fill-extrusion-*` and `projection/axonometric-multiple` failures show occlusion, depth-buffer, or inter-layer 2D/3D ordering differences.
- Pitched fill omission: `regressions/mapbox-gl-js#3320` and `#3702` render expected pitched fills but Skia actual is empty.
- Terrain/color-relief: remaining multi-DEM ocean/bathymetry combinations look like negative-elevation or transparent-stop handling.
- Raster tile LOD: most tile LOD failures preserve visible LOD bands but differ along projected raster edges; `distance-based-scale` additionally shows missing/black horizon coverage.
- Pattern under transforms: line/fill/background pattern failures concentrate around pitch, DPR, uneven sprites, and wrapping.
- Symbol/icon registration: icon-text-fit and pitched/rolled icons are mostly edge sampling, fitted-quad rounding, or subpixel transform differences.
- Heatmap precision: heatmap radius/weight failures are widespread but low-magnitude color-ramp or kernel sampling differences.
- Circle semantics: circle failures split into antimeridian wrapping, sort-key ordering, and pitched circle edge coverage.

## Highest-Leverage Probes

1. Reproduce pitched fill omission with pitch toggled to find the projection/culling threshold.
2. Isolate fill-extrusion depth-buffer semantics before chasing individual extrusion color/translate/projection cases.
3. Split pattern failures with solid-color equivalents to separate geometry/coverage from sprite sampling.
4. Inspect ocean DEM decode and negative color-relief stops before changing general terrain sampling.
5. Compare tile LOD selected tiles before changing raster sampling.
