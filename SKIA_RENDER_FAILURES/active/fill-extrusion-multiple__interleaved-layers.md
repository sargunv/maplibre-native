# render-tests/fill-extrusion-multiple/interleaved-layers

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-multiple/interleaved-layers/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `circle,fill-extrusion`
- Camera: `zoom=18 pitch=60`
- Diff stats: 12997 px, 9.9159%, bbox `(97,38)-(411,208)`

## Observation

Same yellow-over-blue issue as `multiple`, with interleaved circles hidden or exposed differently.

## Likely Issue Class

Interleaved 2D/3D layer ordering and depth-buffer lifetime.

## Evidence

Circle layers sit between extrusion layers, and diffs occur where extrusion overdraw interacts with those circles.

## Suggested Next Probe

Trace depth clear/write state when switching between circle and fill-extrusion layers.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
