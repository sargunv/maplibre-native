> Archived 2026-05-02 after the full Skia sweep passed with near-plane tile clipping fixes.

# render-tests/fill-extrusion-translate-anchor/map

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-translate-anchor/map/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill,fill-extrusion`
- Camera: `zoom=18 pitch=60 bearing=90`
- Diff stats: 633 px, 0.4829%, bbox `(264,119)-(455,227)`

## Observation

Actual omits a narrow cyan wedge expected at the right side.

## Likely Issue Class

Map-anchored translate depth/rotation mismatch.

## Evidence

Diff is small and localized on the map-rotated translated edge; expected-only alpha dominates.

## Suggested Next Probe

Verify map-anchor translate rotates with bearing for both visible and depth geometry.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
