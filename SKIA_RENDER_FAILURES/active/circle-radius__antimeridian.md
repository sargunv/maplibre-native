# render-tests/circle-radius/antimeridian

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/circle-radius/antimeridian/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `circle`
- Camera: `zoom=0`
- Diff stats: 568 px, 0.215%, bbox `(0,237)-(18,276)`

## Observation

Expected splits the circle across both map edges; actual only renders the right-edge fragment and misses the left wrapped sliver.

## Likely Issue Class

Antimeridian/world-wrap handling for circle rendering.

## Evidence

The point is near `[179, 0]` with `circle-radius: 20`; expected has alpha at both edges, actual only at the right edge.

## Suggested Next Probe

Inspect Skia circle bucket/tile wrap handling at zoom 0 for point features near `+/-180`.

## Work Log

- 2026-05-02: Tested disabling Skia tile clipping and then a Skia-local circle wrap-offset draw under the existing tile clip. Neither produced the missing left-edge copy, and the wrap-offset shader probe made the circle mesh invalid, so the likely fix is earlier world-wrap/tile selection or bucket geometry duplication rather than render-pass clipping.
- 2026-05-02: Created from full Skia sweep and circle/heatmap inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
