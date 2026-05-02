# render-tests/fill-extrusion-color/zoom-and-property-function

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-color/zoom-and-property-function/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill-extrusion`
- Camera: `zoom=18 pitch=60`
- Diff stats: 4200 px, 3.2043%, bbox `(57,73)-(342,255)`

## Observation

Colors are broadly correct, but actual has extra magenta/gray overdraw strips where expected is occluded or clean.

## Likely Issue Class

In-layer fill-extrusion feature depth ordering rather than color evaluation.

## Evidence

Composite zoom/property colors appear evaluated; diff is localized to overlapping extrusion faces.

## Suggested Next Probe

Compare feature sort/depth behavior within a single fill-extrusion bucket.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
