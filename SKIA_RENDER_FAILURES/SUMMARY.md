# Skia Render Failure Summary

Baseline: 2026-05-02 full Skia sweep after circle sort-key and line premultiplication fixes, `39 failed`.

## Resolved Clusters

- Tile-mode debug overlays now pass after Skia draws `DebugShader` line and line-strip drawables directly from `DebugUBO`. This archived `map-mode/tile-avoid-edges`, `symbol-placement/*tile-map-mode`, and `text-variable-anchor/*tile-map-mode` workpads.
- Pitched fill omissions now pass after solid fills use the same CPU near-plane clipping as raster meshes and tile clip paths opt out when a tile crosses the near plane. This archived `regressions/mapbox-gl-js#3320` and `#3702`.
- Several fill-extrusion translate/anchor cases also passed after the near-plane tile clip guard: `fill-extrusion-translate/literal`, `fill-extrusion-translate/function`, `fill-extrusion-translate-anchor/viewport`, and `fill-extrusion-translate-anchor/map`.
- `color-relief/low-zoom` and `combinations/color-relief-translucent--hillshade-translucent-low-zoom` passed in the same full sweep and were archived as no-code-change passers.
- `circle-sort-key/literal` now passes after sorted circle features are emitted into separate draw-priority-ordered drawables.
- `line-triangulation/round` now passes after translucent Skia line mesh colors are unpremultiplied before Skia's premultiplied output conversion.

## Current Clusters

- Fill-extrusion depth/order: remaining `fill-extrusion-*` and `projection/axonometric-multiple` failures show opacity, occlusion, depth-buffer, or inter-layer 2D/3D ordering differences.
- Terrain/color-relief: remaining multi-DEM ocean/bathymetry combinations look like negative-elevation or transparent-stop handling.
- Raster tile LOD: most tile LOD failures preserve visible LOD bands but differ along projected raster edges; `distance-based-scale` additionally shows missing/black horizon coverage.
- Pattern under transforms: line/fill/background pattern failures concentrate around pitch, DPR, uneven sprites, wrapping, and dash edge semantics.
- Symbol/icon registration: icon-text-fit and pitched/rolled icons are mostly edge sampling, fitted-quad rounding, or subpixel transform differences.
- Heatmap precision: heatmap radius/weight failures are widespread but low-magnitude color-ramp or kernel sampling differences.
- Circle semantics: circle failures now split between antimeridian wrapping and pitched circle edge coverage.

## Highest-Leverage Probes

1. Isolate remaining fill-extrusion depth-buffer and opacity semantics before chasing individual extrusion color/projection cases.
2. Split pattern failures with solid-color equivalents to separate geometry/coverage from sprite sampling.
3. Inspect ocean DEM decode and negative color-relief stops before changing general terrain sampling.
4. Compare tile LOD selected tiles before changing raster sampling.
5. Investigate the small independent circle and icon registration failures for quick wins.
