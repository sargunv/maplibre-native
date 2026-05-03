# render-tests/background-pattern/pitch

> Archived 2026-05-03 after adding manual perspective correction to the SkSL background-pattern shader and including background pattern drawables in the near-plane projected-triangle clip path.

- Status: passed
- Last sweep: 2026-05-03 (focused background-pattern run)
- Style: `metrics/integration/render-tests/background-pattern/pitch/style.json`
- Layers: `background`
- Camera: `zoom=2 pitch=40`

## Observation

Actual pattern covers only part of the 64x64 viewport and has many transparent/blank areas; expected is fully tiled.

## Likely Issue Class

Pitched background-pattern coverage/clipping or texture wrap setup.

## Evidence

Single background-pattern layer with `pitch: 40`; actual has many more transparent pixels than expected.

## Fix

Two changes applied to `src/mbgl/skia/drawable.cpp`:

1. `backgroundPatternMeshSpecification` now declares an `inv_w` varying and stores `pos_a * inv_w`/`pos_b * inv_w` in vertex output. The fragment shader divides by interpolated `inv_w` before mod/UV mapping, replacing Skia's screen-space-linear varying interpolation with manual perspective correction.
2. The `clipProjectedTriangles` near-plane CPU clipper now triggers for background pattern drawables in addition to raster and solid fills, so foreground tiles whose corners cross the projected near plane are clipped instead of rendering with degenerate `inv_w`.

Without (1), the fragment sampled a wildly wrong `pos_a` along the perspective direction, producing the noisy aliased pattern observed in the actual image. Without (2), foreground tiles with at least one corner past the near plane produced empty fragments.

## Work Log

- 2026-05-03: Applied manual perspective correction in `backgroundPatternMeshSpecification` and extended near-plane clip path to background pattern drawables. Focused `render-tests/background-pattern/` now passes 6/6.
- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
