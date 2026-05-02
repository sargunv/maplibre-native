> Archived 2026-05-02 after the full Skia sweep passed with near-plane tile clipping fixes.

# render-tests/fill-extrusion-translate-anchor/viewport

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-translate-anchor/viewport/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill,fill-extrusion`
- Camera: `zoom=18 pitch=60 bearing=90`
- Diff stats: 4486 px, 3.4225%, bbox `(263,94)-(455,202)`

## Observation

Actual omits an expected cyan wedge behind/right of the extrusion.

## Likely Issue Class

Viewport-anchored translate occlusion or depth mismatch.

## Evidence

Most differing pixels are expected-only alpha over cyan; the visual geometry is localized to the translated edge.

## Suggested Next Probe

Compare viewport-anchor translate matrix before and after bearing rotation.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
