# render-tests/icon-offset/pitched-offset

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/icon-offset/pitched-offset/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,line,symbol`
- Camera: `zoom=14 pitch=60`
- Diff stats: 10575 px, 1.9708%, bbox `(133,0)-(370,1047)`

## Observation

Roads and arrows broadly align, but diffs outline pitched road edges, center dashes, and arrows through the full height.

## Likely Issue Class

Projected line/icon offset rounding under pitched map alignment.

## Evidence

The style uses line placement and `icon-offset` values including `+50.34` and `-53.34`; diff stays on the projected road corridor.

## Suggested Next Probe

Compare projected anchor positions for line symbols with positive and negative `icon-offset` at pitch `60`.

## Work Log

- 2026-05-02: Created from full Skia sweep and symbol inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
