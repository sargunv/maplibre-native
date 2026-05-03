# render-tests/real-world/nepal

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/real-world/nepal/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `local://styles/nepal.json`
- Camera: `zoom=13`, `center=[85.48805250000001, 28.115547]`
- Diff stats: `low-magnitude linework diffs, bbox spans scattered terrain contours`

## Observation

The terrain and lake broadly match expected, but scattered contour/linework pixels differ across the map. The actual image shows slightly thinner or shifted contour strokes in several terrain bands.

## Likely Issue Class

Tile-heavy vector line rendering or LOD/source selection in the real-world Nepal style.

## Evidence

The style delegates to `local://styles/nepal.json` at zoom 13. The diff is sparse and follows contour linework rather than fill or raster regions, which matches the broader tile-heavy/vector-line failure cluster.

## Suggested Next Probe

Compare the local Nepal style layers and tile IDs selected by Skia versus Metal, then isolate contour line layers with a focused style to separate LOD/source selection from line rasterization.

## Work Log

- 2026-05-02: Added missing active workpad from the full Skia sweep; failure was present in the 39-failure run after circle sort-key and line premultiplication fixes.

## Resolution

Move this file to `archive/<YYYY-MM-DD>/` after the test passes a focused run and the next full sweep.
