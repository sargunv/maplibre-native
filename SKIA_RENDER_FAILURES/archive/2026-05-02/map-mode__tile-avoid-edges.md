> Archived 2026-05-02 after focused and full sweeps passed with Skia debug line-strip rendering.

# render-tests/map-mode/tile-avoid-edges

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/map-mode/tile-avoid-edges/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: style import
- Camera: none
- Diff stats: 20246 px, 7.7232%, bbox `(85,0)-(511,313)`

## Observation

Map content mostly matches, but expected has a large pure-red tile/debug border along left and bottom edges; actual is missing it.

## Likely Issue Class

Debug/tile-boundary overlay rendering or clipping in Skia tile mode.

## Evidence

Style enables `mapMode: "tile"`, `collisionDebug: true`, and `debug: true`; inspection found expected red pixels absent from actual.

## Suggested Next Probe

Trace Skia rendering of tile debug boundaries in tile mode, especially edge clipping/scissor behavior for non-origin tile viewports.

## Work Log

- 2026-05-02: Created from full Skia sweep and miscellaneous inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
