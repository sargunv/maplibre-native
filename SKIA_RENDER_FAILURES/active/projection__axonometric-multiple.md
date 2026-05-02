# render-tests/projection/axonometric-multiple

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/projection/axonometric-multiple/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill-extrusion`
- Camera: `zoom=18 pitch=60`
- Diff stats: 4006 px, 3.0563%, bbox `(106,106)-(330,202)`

## Observation

Actual yellow side/front face protrudes through blue, while expected blue occludes it.

## Likely Issue Class

Same fill-extrusion depth/order issue as `fill-extrusion-multiple`, under axonometric mode.

## Evidence

Layer setup matches the multiple extrusion scenario and uses `metadata.test.axonometric: true`.

## Suggested Next Probe

Check whether axonometric rendering changes or bypasses depth ordering for extrusion batches.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
