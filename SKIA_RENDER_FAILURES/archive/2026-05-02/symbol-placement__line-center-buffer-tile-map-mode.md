> Archived 2026-05-02 after focused and full sweeps passed with Skia debug line-strip rendering.

# render-tests/symbol-placement/line-center-buffer-tile-map-mode

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/symbol-placement/line-center-buffer-tile-map-mode/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,symbol,line`
- Camera: `zoom=4`
- Diff stats: 9160 px, 0.8736%, bbox `(0,512)-(1023,1023)`

## Observation

The `Caribbean Sea` label and line mostly match. Expected has a red tile boundary/grid overlay in the lower half that actual lacks.

## Likely Issue Class

Missing tile debug overlay rather than line-center placement.

## Evidence

Diff is concentrated on lower tile boundaries, and red debug pixels are absent from actual.

## Suggested Next Probe

Verify debug overlay rendering when symbols cross tile buffers.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
