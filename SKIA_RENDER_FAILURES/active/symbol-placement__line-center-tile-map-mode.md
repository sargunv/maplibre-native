# render-tests/symbol-placement/line-center-tile-map-mode

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/symbol-placement/line-center-tile-map-mode/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,symbol,line`
- Camera: `zoom=15`
- Diff stats: 38790 px, 3.6993%, bbox `(0,0)-(1023,1023)`

## Observation

Road geometry and collision circles mostly match. Expected contains red debug tile boundaries that actual mostly lacks.

## Likely Issue Class

Missing debug/tile-boundary rendering in Skia tile map mode.

## Evidence

Inspection found strong red pixels are far fewer in actual than expected. The failure spans the full image because the missing debug grid spans tile edges.

## Suggested Next Probe

Trace `debug: true` and `mapMode: "tile"` tile-boundary drawing in the Skia backend before investigating symbol placement.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
