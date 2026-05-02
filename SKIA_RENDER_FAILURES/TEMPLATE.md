# render-tests/<category>/<name>

- Status: active
- Last sweep: 2026-MM-DD
- Style: `metrics/integration/render-tests/<category>/<name>/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `<layer types>`
- Camera: `<zoom/pitch/bearing/roll>`
- Diff stats: `<changed pixels, percent, bbox>`

## Observation

Describe the visible difference between actual and expected.

## Likely Issue Class

Name the subsystem or semantic class this failure appears to exercise.

## Evidence

Record concrete facts: style properties, image statistics, changed-pixel location, previous experiments, and links to related files.

## Suggested Next Probe

List the smallest experiment or diagnostic that can falsify the current hypothesis.

## Work Log

- 2026-MM-DD: Created from full Skia sweep.

## Resolution

Move this file to `archive/<YYYY-MM-DD>/` after the test passes a focused run and the next full sweep.
