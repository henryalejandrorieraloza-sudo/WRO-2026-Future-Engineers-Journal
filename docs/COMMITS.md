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
| `e8cc71d` | 2026-11-23 | 279,446 | 76,080 | 100.0 % | Reference for the count (before the protrusion fixes). Later small code commits: unused constants removed (`8589d11`), `REVERSE_TARGET_TICKS` set to 597 (`23d6055`), the `turn_measure_moving` test added (`1a055bd`), comments edited. |

## Result for the first commit

- The first commit (`f3a9770`) contains **no code**, so it does not meet the 1/5 requirement under either count. This cannot be changed: the git history is not rewritten.
- The colour-detection method (HSV thresholds and BFS blob grouping) is described in the engineering journal in that first commit. The code for it first appears in `5baf921`, minutes later on the same day.
- The first commit with at least 1/5 of the final code is `814ddcf` (31.2 % by the total count, 21.3 % by .ino alone), the same day as the first commit.

## Interpretation to confirm with the organisers

The rule text asks that "the first commit" contain at least 1/5 of the final code. Reading it as "the repository reached 1/5 by the first deadline" (which `814ddcf` does, on 4 Oct) is plausible, but it is not the literal wording. This should be confirmed with the organisers when the repository link is sent.

## Deadlines

| Requirement | Deadline | Commit that meets it |
|---|---|---|
| First commit with ≥ 1/5 of code | 8 Oct 2026 | `814ddcf` (4 Oct 2026, 31.2 %, same day as the first commit). Note: `f3a9770` is the first commit in the history and contains no code. |
| Second commit | 8 Nov 2026 | `06f5685` (5 Nov) |
| Third commit | 24 Nov 2026 | `e8cc71d` (23 Nov) and the commits after it (the small code commits listed above and the documentation commits), up to the v1.9.19 review of 24 Nov (documents and comments only, no code logic). The rules may not score changes made after 24 Nov |

The rules note that changes after the third commit may not be scored, so documentation must be complete in the repository by that commit.

## Size of the history

200 commits on `main` at commit `3f30eca` (24 Nov 2026, `git rev-list --count main`), 110 of them by the Claude account and 5 more marked "Henry Riera (via Claude)" (see README §15). Merge commits from pull requests are included in the count.

## Current figures (24 Nov 2026)

- `main` at `f3d1e0a` (24 Nov 2026, before the review of this day): **231 commits** (`git rev-list --count main`). **131** have the author name Claude, and **5** more are marked "Henry Riera (via Claude)".
- Code (`.ino` + `.html`, all tracked files) at `3e13b68`: **348,149 bytes**. 1/5 of it = **69,629 bytes**.
- First commit with code, `814ddcf`: **110,854 bytes = 31.8 %** of that code. The 1/5 rule is met.
- These figures change after the final merge. Recount before the deadline with `git rev-list --count` and the byte totals above.

## Question to the organisers (pending)

Status: **not sent yet, no answer yet.** Send this to the organisers and paste their answer below.

> Subject: WRO 2026 Future Engineers, team X67: two questions on the repository requirements
>
> Dear WRO organisers,
>
> We are team X67 (Colegio Americano de Guayaquil, Ecuador). Our public repository is https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal
>
> 1. **First commit.** Rule 7 asks that the first commit, not later than two months before the competition, contains at least 1/5 of the final code. The first commit in our history (`f3a9770`, 4 Oct 2026) contains only the engineering journal and the test log. The first commit with code (`814ddcf`, same day) contains 31 % of the final code. Is the commit with at least 1/5 of the code accepted as the first commit, or must the first commit itself contain it?
> Could you also confirm the dates of the International Final? Our deadlines are calculated from 8 December 2026.
>
> Thank you,
> Team X67, Henry Riera Loza

**Answer from the organisers:** pending.
