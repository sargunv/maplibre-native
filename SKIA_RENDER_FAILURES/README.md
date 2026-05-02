# Skia Render Failure Workpads

This directory tracks active Skia render-test failures as individual workpads. The reader is a contributor debugging Skia parity, and the job is to keep visual evidence, hypotheses, and next probes close to each failing test.

## Layout

```text
SKIA_RENDER_FAILURES/
  README.md       # directory purpose and current sweep baseline
  WORKFLOW.md     # update, archive, and inspection process
  TEMPLATE.md     # per-test workpad template
  SUMMARY.md      # aggregate patterns from active workpads
  active/         # one markdown file per currently failing test
  archive/        # passed or deferred historical workpads
```

## Baseline

- Sweep date: 2026-05-02
- Command: `/usr/bin/time -p build-macos-skia/mbgl-render-test-runner --manifestPath metrics/macos-skia.json`
- Result: `1258 passed`, `12 ignored-passed`, `79 ignored`, `53 failed`
- Full output: `/Users/sargunv/.local/share/opencode/tool-output/tool_dea079e32001JZ8Z68lgOO8CjE`
- Generated report: `metrics/macos-skia.html`

The active workpads are based on the generated `actual.png`, `expected.png`, and `diff.png` artifacts from that sweep.

## Active Policy

Keep one active markdown file for each non-ignored failing Skia render test. When a test passes in a focused run and remains passing in the next full sweep, move its workpad into `archive/<YYYY-MM-DD>/` and add a short archive note with the fixing commit.
