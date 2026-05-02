> Archived 2026-05-02 after the full Skia sweep passed with near-plane tile clipping fixes.

# render-tests/fill-extrusion-translate/literal

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-extrusion-translate/literal/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `fill,fill-extrusion`
- Camera: `zoom=19 pitch=60`
- Diff stats: 22703 px, 17.321%, bbox `(64,20)-(404,255)`

## Observation

Actual hides much more cyan fill under and around translated extrusions; expected leaves cyan base visible.

## Likely Issue Class

Fill-extrusion translate applied inconsistently between visible geometry and occlusion/depth footprint.

## Evidence

Style uses `fill-extrusion-translate: [-30, -30]`; diff shows large missing cyan regions.

## Suggested Next Probe

Check the Skia extrusion translate path against GL depth mask and draw order behavior.

## Work Log

- 2026-05-02: Created from full Skia sweep and extrusion inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
