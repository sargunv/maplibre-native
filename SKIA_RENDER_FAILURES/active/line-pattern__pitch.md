# render-tests/line-pattern/pitch

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/line-pattern/pitch/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,line`
- Camera: `zoom=17 pitch=60`
- Diff stats: 40240 px, 30.7007%, bbox `(0,0)-(511,255)`

## Observation

Actual is effectively white where expected has semi-transparent gray patterned road network.

## Likely Issue Class

Pitched line-pattern draw is dropped, fully clipped, or transparent.

## Evidence

Style uses `pitch: 60`, `line-width: 20`, `line-pattern: generic_icon`, and `line-opacity: 0.5`.

## Suggested Next Probe

Run the same style with `pitch: 0`, then with solid `line-color` instead of `line-pattern` to isolate geometry vs pattern shader/sprite sampling.

## Work Log

- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
