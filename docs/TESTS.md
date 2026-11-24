# Test log

This file records how each test was run and what was measured. One row per run. Keep failed runs: the rubric rewards iteration, not only the final result.

Rows are added after each run, using the final code of the commit shown.

## Three-lap run (Open Challenge)

Procedure: place the car at the start of the Open Challenge track, start the round, and record each run.

Pass criterion: three laps, then a stop inside the starting section.

Target (team plan, not a rubric requirement): at least 20 runs with the final code, so the success rate can be compared between sessions. The rubric asks for testing and iteration evidence with metrics, not a specific number of runs.

| Run | Date | Commit | Battery (V) | Laps completed | Success (yes / no) | Time per lap (s) | Direction | Stop position | Notes |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 22 Nov 2026 | `3010306` or `c3441cd` (both on 22 Nov, 21:35 and 21:39; which one ran is not recorded) | not recorded | 3 (12 corners) | yes | not recorded | not recorded | Starting section, close to the start wall (not the exact spot) | First attempt, no hand intervention. Corridors narrow. Source: journal §19.2 |

## Summary

Only one run is recorded so far. The 20-run series is still to be done; the fields marked "not recorded" were not written down at the time.

| Test | Runs | Successes | Success rate | Median time per lap (s) |
|---|---:|---:|---:|---:|
| Open Challenge, 3 laps | 1 | 1 | 100 % (1 run so far) | not recorded |

## Obstacle Challenge (driving program `obstacle_challenge`, without parking)

Procedure: place the car at the start of the Obstacle track with the pillars in place, press START, and record each run. The car was run on the track on 23 Nov (team report), but no run has been written down with its data yet.

Columns to fill for each run: lap times, direction of the first corner (blue = left, orange = right), stop position (from the final report line `>>> FINAL`), pillars passed on the correct side (yes / no for each), and notes.

| Run | Date | Commit | Battery (V) | Laps completed | Success (yes / no) | Time per lap (s) | Direction | Stop position | Pillars on correct side | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| — | — | — | — | — | — | — | — | — | — | No run recorded yet |

Edge cases the team checked (23 Nov, team report) are not listed in the repository. For each one, add a row with the case, the commit tested and the result (pass / fail, with what happened).

| Edge case | Commit | Result | Notes |
|---|---|---|---|
| Not listed yet | — | not recorded | — |

## Other tests

Add a section for each test: motor (torque and speed), camera colour detection, ToF sensors, gyroscope turn. Use the same columns as above, and state the commit of the code tested.
