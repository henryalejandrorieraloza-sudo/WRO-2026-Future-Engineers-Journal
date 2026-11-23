# Commit history and the 1/5 rule

The WRO 2026 rules require at least three commits: the first no later than two months before the competition and containing at least 1/5 of the final code; the second no later than one month before; the third no later than two weeks before. The deadlines below assume the international final on **8 December 2026**, which is consistent with the link deadline of 17 November (three weeks before).

## How code is counted

- Code = `.ino` files plus `.html` files (the viewer). Final code at commit `e8cc71d`: **279,446 bytes of .ino + 76,080 bytes of .html = 355,526 bytes**.
- 1/5 of the final code = **71,105 bytes** (both kinds counted). Counting .ino only, 1/5 = 55,889 bytes.

## Key commits

| Commit | Date | .ino bytes | .html bytes | Share of final (.ino + .html) | Note |
|---|---|---:|---:|---:|---|
| `f3a9770` | 2026-10-04 | 0 | 0 | 0.0 % | First commit. Contains only ENGINEERING_JOURNAL.md and TESTS.md. No code. The colour-detection method (HSV + BFS) is described in the journal text of this commit. |
| `5baf921` | 2026-10-04 | 28,142 | 0 | 7.9 % | First commit with code (color_detection_test.ino and the first tests), minutes after f3a9770. |
| `814ddcf` | 2026-10-04 | 59,441 | 51,413 | 31.2 % | Adds the pillar viewer and the HTML viewer. |
| `b2abe4a` | 2026-10-04 | 31,299 | 51,413 | 23.3 % | Last commit of 4 Oct. Code removed in d10690d and reorganised in 3c4ca50. |
| `a6ada36` | 2026-10-07 | 37,165 | 114,170 | 42.6 % | Last commit before 8 Oct (2 months before 8 Dec 2026). |
| `06f5685` | 2026-11-05 | 197,998 | 114,170 | 87.8 % | Last commit before 8 Nov (1 month before 8 Dec 2026). |
| `e8cc71d` | 2026-11-23 | 279,446 | 76,080 | 100.0 % | Current state at the time of this document (before the protrusion fixes). |

## Result for the first commit

- The first commit (`f3a9770`) contains **no code**, so it does not meet the 1/5 requirement under either count. This cannot be changed: the git history is not rewritten.
- The colour-detection method (HSV thresholds and BFS blob grouping) is described in the engineering journal in that first commit. The code for it first appears in `5baf921`, minutes later on the same day.
- The first commit with at least 1/5 of the final code is `814ddcf` (31.2 % by the total count, 21.3 % by .ino alone), the same day as the first commit.

## Deadlines

| Requirement | Deadline | Commit that meets it |
|---|---|---|
| First commit with ≥ 1/5 of code | 8 Oct 2026 | `814ddcf` (4 Oct 2026, 31.2 %, same day as the first commit). Note: `f3a9770` is the first commit in the history and contains no code. |
| Second commit | 8 Nov 2026 | `06f5685` (5 Nov) |
| Third commit | 24 Nov 2026 | `e8cc71d` (23 Nov) and later commits |

The rules note that changes after the third commit may not be scored, so documentation must be complete in the repository by that commit.

