# Skia Render Failure Workflow

Use this workflow when the Skia render-test failure set changes or when a failure is investigated.

## Refresh The Corpus

1. Run the full Skia render sweep:
   ```sh
   /usr/bin/time -p build-macos-skia/mbgl-render-test-runner --manifestPath metrics/macos-skia.json
   ```
2. Extract the `* failed render-tests/...` list from the full output.
3. Ensure `active/` has one markdown file for each failing test.
4. Create new files from `TEMPLATE.md` for new failures.
5. Move resolved files to `archive/<YYYY-MM-DD>/` after they pass a full sweep.
6. Update `README.md` with the sweep date, result counts, and output path.
7. Update `SUMMARY.md` with any changed clusters or priorities.

## Inspect A Failure

For each active failure, inspect these files under `metrics/integration/render-tests/<test>/`:

- `style.json`
- `actual.png`
- `expected.png`
- `diff.png`

Record what differs visually, the likely issue class, concrete evidence, and the next probe. Prefer small, falsifiable probes over broad rewrites.

## Use Read-Only Batches

When the failure set is large, split tests by subsystem and run read-only inspection batches before editing workpads. Useful groups are symbol/icon, fill-extrusion/projection, terrain/raster, line/pattern, circle/heatmap, and miscellaneous regressions.

The batch output should answer four questions for each test:

- What does actual do differently from expected?
- What subsystem or semantic class does this suggest?
- What evidence supports that classification?
- What is the smallest next probe?

## Archive Rules

Move a workpad from `active/` to `archive/<YYYY-MM-DD>/` when both are true:

- The test passes in the focused filter used for the fix.
- The test remains passing in the next full Skia sweep.

Add an archive note at the top of the moved file:

```md
> Archived 2026-MM-DD after `<commit>` fixed <short reason>.
```

Do not delete the workpad. Historical notes are useful when related failures reappear.

## Commit Hygiene

Commit workpad updates with the code or investigation milestone that made them accurate. Delete generated `metrics/macos-skia.html` before commits unless the report is intentionally part of the change.
