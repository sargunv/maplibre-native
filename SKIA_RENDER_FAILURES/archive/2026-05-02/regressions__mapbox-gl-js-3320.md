> Archived 2026-05-02 after focused and full sweeps passed with solid-fill near-plane clipping and tile-clip near-plane guard.

# render-tests/regressions/mapbox-gl-js#3320

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/regressions/mapbox-gl-js#3320/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill`
- Camera: `pitch=40 bearing=20`
- Diff stats: 3363 px, 82.1045%, bbox `(0,5)-(63,63)`

## Observation

Expected shows a semi-transparent blue pitched polygon covering most of the image; actual is transparent/empty.

## Likely Issue Class

Pitched fill rendering or culling failure.

## Evidence

Single GeoJSON fill layer with `fill-opacity: 0.5`; actual has only transparent pixels.

## Suggested Next Probe

Run the same style with `pitch: 0` and `bearing: 0`; if it appears, inspect fill vertex projection/frustum clipping for pitched fills.

## Work Log

- 2026-05-02: Created from full Skia sweep and miscellaneous inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
