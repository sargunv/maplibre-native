# render-tests/background-pattern/pitch

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/background-pattern/pitch/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background`
- Camera: `zoom=2 pitch=40`
- Diff stats: 3844 px, 93.8477%, bbox `(0,0)-(63,63)`

## Observation

Actual pattern covers only part of the 64x64 viewport and has many transparent/blank areas; expected is fully tiled.

## Likely Issue Class

Pitched background-pattern coverage/clipping or texture wrap setup.

## Evidence

Single background-pattern layer with `pitch: 40`; actual has many more transparent pixels than expected.

## Suggested Next Probe

Render the same background with `pitch: 0`, then inspect pitched background quad extent and sampler wrap/clamp behavior.

## Work Log

- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
