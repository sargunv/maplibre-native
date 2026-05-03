# Skia Render Failure Summary

Baseline: 2026-05-03 full Skia sweep after background-pattern perspective correction, expected `38 failed` (one less than the 2026-05-02 baseline).

## Resolved Clusters

- Tile-mode debug overlays now pass after Skia draws `DebugShader` line and line-strip drawables directly from `DebugUBO`. This archived `map-mode/tile-avoid-edges`, `symbol-placement/*tile-map-mode`, and `text-variable-anchor/*tile-map-mode` workpads.
- Pitched fill omissions now pass after solid fills use the same CPU near-plane clipping as raster meshes and tile clip paths opt out when a tile crosses the near plane. This archived `regressions/mapbox-gl-js#3320` and `#3702`.
- Several fill-extrusion translate/anchor cases also passed after the near-plane tile clip guard: `fill-extrusion-translate/literal`, `fill-extrusion-translate/function`, `fill-extrusion-translate-anchor/viewport`, and `fill-extrusion-translate-anchor/map`.
- `color-relief/low-zoom` and `combinations/color-relief-translucent--hillshade-translucent-low-zoom` passed in the same full sweep and were archived as no-code-change passers.
- `circle-sort-key/literal` now passes after sorted circle features are emitted into separate draw-priority-ordered drawables.
- `line-triangulation/round` now passes after translucent Skia line mesh colors are unpremultiplied before Skia's premultiplied output conversion.
- `background-pattern/pitch` now passes after the SkSL background-pattern shader does manual perspective correction on `pos_a`/`pos_b` and the near-plane projected-triangle clipper covers background pattern drawables.

## Current Clusters

- Fill-extrusion depth/order: remaining `fill-extrusion-*` and `projection/axonometric-multiple` failures show opacity, occlusion, depth-buffer, or inter-layer 2D/3D ordering differences. Documented as the acceptable deferral class per the parity target.
- Terrain/color-relief: remaining multi-DEM ocean/bathymetry combinations look like negative-elevation or transparent-stop handling.
- Raster tile LOD: most tile LOD failures preserve visible LOD bands but differ along projected raster edges; `distance-based-scale` additionally shows missing/black horizon coverage.
- Pattern under transforms: remaining line/fill pattern failures (`line-pattern/pitch`, `line-pattern/@2x`, `fill-pattern/uneven-pattern`, `fill-pattern/wrapping-with-interpolation`). `line-pattern/pitch` is visually fixed by the same perspective-correction approach used for background-pattern but still fails the strict pixelmatch threshold; the remaining diff is sub-pixel anti-aliasing precision (likely needs gamma_scale equivalent for blur). `fill-pattern/wrapping-with-interpolation` is an MLT vs MVT geometry decoding diff, not a pattern shader bug.
- Symbol/icon registration: icon-text-fit and pitched/rolled icons are mostly edge sampling, fitted-quad rounding, or subpixel transform differences.
- Heatmap precision: heatmap radius/weight failures show very subtle gradient diffs (the actual.png and expected-half-float.png are visually nearly identical), but pixelmatch reports large counts that don't visually correspond to obvious red specks in diff.png.
- Circle semantics: circle failures now split between antimeridian wrapping and pitched circle edge coverage.

## Highest-Leverage Probes

1. Add a vertex-shader gamma_scale equivalent to line-pattern and lineSDF and re-apply the perspective correction on top. SkSL mesh fragments do not expose `dFdx`/`dFdy`/`fwidth`, so the screen-space derivative trick fails at spec compile; the formula has to come from `inv_w` and the line direction in the vertex shader.
2. Isolate remaining fill-extrusion depth-buffer and opacity semantics before chasing individual extrusion color/projection cases.
3. Inspect ocean DEM decode and negative color-relief stops before changing general terrain sampling.
4. Compare tile LOD selected tiles before changing raster sampling.
5. Investigate the small independent circle and icon registration failures for quick wins.

## Failure Margin Survey (2026-05-03)

A diff-pixel counter run against the current sweep ranks failures by how far they are from the 0.015% pixelmatch threshold. Every "red" pixel in `diff.png` counts; "yellow" anti-alias pixels do not.

Closest to passing (likely sub-pixel precision class):

- `icon-roll-alignment/auto-rotation-alignment-map`: 1 red px, threshold ≈0.6 px on a 64×64 image. Even a single red pixel exceeds the allowed 0.015%, so the test demands pixel-perfect rendering.
- `line-pattern/@2x`: 7 red, threshold 2 (margin +5). Dash-boundary precision at retina pixel ratio.
- `heatmap-radius/literal`: 14 red, threshold 4 (margin +10). Color-band transition pixels at heatmap contour edges.
- `heatmap-radius/function` (margin +34), `icon-text-fit/enlargen-both` (+48), `heatmap-weight/identity-property-function` (+56), `line-dasharray/zero-length-gap` (+73), `heatmap-radius/pitch30` (+87), `circle-pitch-alignment/*` (+114, +211), `icon-text-fit/both-text-anchor-1x-image-2x-screen` (+217), `fill-pattern/wrapping-with-interpolation` (+350), `icon-text-fit/both-text-anchor-2x-image-2x-screen` (+353), `real-world/nepal` (+405), `circle-radius/antimeridian` (+477).
- Larger margins are largely the depth/3D deferral class (`fill-extrusion-*`, `projection/*`) plus the tile-lod cluster.

## Constraints Discovered While Probing

- **SkSL mesh fragments do not expose `dFdx`/`dFdy`/`fwidth`**, even though the language declares them. Spec creation fails with `no match for fwidth(float)`. Any GL feature that depends on screen-space derivatives — most importantly `gamma_scale` for line-edge AA under perspective — needs a vertex-shader-only formulation that derives the same factor from `inv_w` and the un-extruded line direction, or a per-vertex precomputed extrude-length attribute. The line-pattern bucket already bakes the extrusion into `a_pos`, so reconstructing the perpendicular direction in the vertex shader requires extra plumbing.
- **`SkMeshSpecification` allows at most 6 varying slots.** Adding `inv_w` to a shader that already declares 5 varyings forces packing it into an unused float lane of an existing varying (e.g., `line_b.z` for line-pattern).
- **`WrapCAMetalLayer` writes the drawable handle inside Skia's lazy proxy callback**, which fires during `flushAndSubmit`, not during the wrap call. The out-pointer must outlive `flushAndSubmit`; passing the address of a stack-local results in null drawables every frame. The GLFW Skia present path now stores `liveDrawable` in the `RenderableResource` so it stays valid across the wrap+submit boundary.
- **Switching the heatmap accumulation `OffscreenTexture` to HalfFloat regressed all heatmap tests**, even though that channel type is what the renderer requested. The heatmap kernel writes `(val, 1, 1, 1)` premultiplied; with the half-float surface and `kPremul_SkAlphaType`, accumulated values come back so wrong that the colorize pass renders blue/cyan instead of pink. The interaction between Skia's color management on a `kPremul kRGBA_F16` surface and additive-blended output values > 1 is the open question; until that's understood the RGBA8 path is what produces the small per-pixel diffs we currently see.
- **`Context::createOffscreenTexture` discards the requested `gfx::TextureChannelDataType`** in the current Skia path. Threading it through is straightforward but doesn't pay off until the half-float interaction above is resolved.
