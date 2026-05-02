# render-tests/combinations/color-relief--hillshade--color-relief--hillshade

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/combinations/color-relief--hillshade--color-relief--hillshade/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,color-relief,hillshade`
- Camera: `zoom=1`
- Diff stats: 62247 px, 94.9814%, bbox `(0,0)-(255,255)`

## Observation

Actual loses most dark blue bathymetry/ocean relief; ocean areas become greenish or background-like while land terrain still renders.

## Likely Issue Class

Ocean DEM negative-elevation decode, color-relief transparent stop handling, or multi-DEM compositing.

## Evidence

Style has separate `.ocean.webp` DEM sources with negative elevation color stops ending at transparent `0`; signed diff has much higher green and lower blue in actual.

## Suggested Next Probe

Probe `.ocean.webp` DEM decode and negative elevation values in Skia, then verify transparent stop handling around elevation `0`.

## Work Log

- 2026-05-02: Created from full Skia sweep and terrain inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
