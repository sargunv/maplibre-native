# render-tests/fill-extrusion-multiple/multiple

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-multiple/multiple/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill-extrusion`
- Camera: `zoom=18 pitch=60`
- Diff stats: 12963 px, 9.89%, bbox `(97,38)-(333,202)`

## Observation

Actual yellow extrusion side draws through or over blue where expected blue occludes the lower yellow.

## Likely Issue Class

Cross-layer fill-extrusion depth ordering.

## Evidence

Three separate extrusion layers are involved; the mismatch follows overlapping side faces, not color evaluation.

## Suggested Next Probe

Check whether Skia batches fill-extrusion layers without preserving shared depth behavior.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
