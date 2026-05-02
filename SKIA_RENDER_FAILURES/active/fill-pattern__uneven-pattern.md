# render-tests/fill-pattern/uneven-pattern

- Status: active
- Last sweep: 2026-05-02
- Style: `metrics/integration/render-tests/fill-pattern/uneven-pattern/style.json`
- Artifacts: `actual.png`, `expected.png`, `diff.png`
- Layers: `background,fill`
- Camera: `zoom=2`
- Diff stats: 18434 px, 7.032%, bbox `(0,0)-(511,511)`

## Observation

Pattern interior mostly matches; diffs concentrate along coast/fill boundaries and some full-image pattern alignment edges.

## Likely Issue Class

Fill edge antialiasing, polygon coverage, or tile-boundary pattern placement with uneven sprite dimensions.

## Evidence

Pattern colors are nearly identical; changed pixels outline Africa/coastline and pattern edges.

## Suggested Next Probe

Compare the same water geometry with solid fill to isolate coverage AA, then inspect tile-boundary pattern origin for uneven sprite dimensions.

## Work Log

- 2026-05-02: Created from full Skia sweep and line/pattern inspection batch.

## Resolution

Move to `archive/<YYYY-MM-DD>/` after a focused pass and full sweep pass.
