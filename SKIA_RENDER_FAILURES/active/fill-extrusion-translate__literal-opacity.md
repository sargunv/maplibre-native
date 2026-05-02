# render-tests/fill-extrusion-translate/literal-opacity

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-translate/literal-opacity/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill,fill-extrusion`
- Camera: `zoom=19 pitch=60`
- Diff stats: 76864 px, 58.6426%, bbox `(0,0)-(408,255)`

## Observation

Actual translucent extrusion darkens and occludes far more cyan than expected.

## Likely Issue Class

Transparent fill-extrusion compositing/depth interaction with translated geometry.

## Evidence

Style uses `fill-extrusion-opacity: 0.5`; diff covers a broad translucent footprint.

## Suggested Next Probe

Probe whether translucent fill-extrusions still write depth or use untranslated depth bounds in Skia.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
