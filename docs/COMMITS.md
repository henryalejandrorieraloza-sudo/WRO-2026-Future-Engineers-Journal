# Commit history and the 1/5 rule

The WRO 2026 rules require at least three commits: the first no later than two months before the competition and containing at least 1/5 of the final code; the second no later than one month before; the third no later than two weeks before. The deadlines below assume the international final on **8 December 2026**, which is consistent with the link deadline of 17 November (three weeks before).

## Reference figures

All figures use one reference commit: **`c1fa640`** (24 Nov 2026, merge of PR #53).

- Commits on `main` at `c1fa640`: **242** (`git rev-list --count c1fa640`). Of these, **137** carry the author name Claude, and **5** more are marked "Henry Riera (via Claude)".
- Code at `c1fa640`: all tracked `.ino` and `.html` files, **331,624 bytes**. 1/5 of it = **66,325 bytes**.
- First commit with code, `814ddcf` (4 Oct 2026): **110,854 bytes = 33.4 %** of the code at `c1fa640`. The 1/5 rule is met.

## Snapshot for the third commit

- The state of `main` on 24 Nov 2026 is frozen with the tag **`rubric-snapshot-24nov-2026`** (commit `eaf2576`, 21:35 local time). The judges' third commit is this state. Later changes go in later commits and may not be scored.
- The tag points to an existing commit; no history was rewritten.
- **Final snapshot for scoring:** the commit that adds this text is the team's third commit for scoring. It was made on 24 Nov 2026, before midnight local time (America/Guayaquil), so before the 24 Nov deadline, and is tagged **`rubric-snapshot-final-24nov-2026`**. It contains the corrected Obstacle flowchart (README §9.11, 0.45 s and 20 cm) and the first-commit reading above. The earlier tag `rubric-snapshot-24nov-2026` (`eaf2576`) remains as the earlier state.

## Notes on the early history

- `f3a9770` and `5baf921` are titled "Add files via upload", and `814ddcf` too. Their titles are not descriptive; the history is kept as it is, and the commits that matter for the rule are listed in the table above.
- Measure the commits with the byte count above (`.ino` and `.html` only). Paths in this repository contain spaces (`WRO code/`, `WRO diario/`), so split the `git ls-tree` output on the tab character (`awk -F'\t'`), not on whitespace, or the sizes come out as zero.

Recount with the commands below before the deadline. These figures change after any later code commit.

```
git rev-list --count c1fa640
git ls-tree -r -l c1fa640 | awk -F'\t' '$2 ~ /\.(ino|html)$/ {split($1,a," "); s+=a[4]} END {print s}'
```

## Key commits

| Commit | Date | Code bytes (.ino + .html) | Share of code at `c1fa640` | Note |
|---|---|---:|---:|---|
| `f3a9770` | 2026-10-04 | 0 | 0.0 % | Literal first commit. Contains only the engineering journal and the test log. No code. |
| `814ddcf` | 2026-10-04 | 110,854 | 33.4 % | First commit with code (same day). Adds the pillar viewer and the HTML viewer. |
| `06f5685` | 2026-11-05 | 312,168 | 94.1 % | Second-deadline commit (1 month before 8 Dec 2026). |
| `c1fa640` | 2026-11-24 | 331,624 | 100.0 % | Reference commit for every figure in this file. |

## Result for the first commit

- The literal first commit (`f3a9770`) contains **no code**, so it does not meet the 1/5 requirement by itself. The git history is not rewritten, so this cannot be changed.
- The colour-detection method (HSV thresholds and BFS blob grouping) is described in the engineering journal in that first commit. The code for it first appears in the same day's commits.
- The first commit with at least 1/5 of the final code is `814ddcf` (33.4 %), the same day as the first commit.
- **Team reading of the rule:** the first commit, due not later than 8 Oct 2026 (two months before the 8 Dec 2026 final), must contain at least 1/5 of the final code. That condition is met on 4 Oct 2026 by `814ddcf`, 110,854 bytes = 33.4 % of the code at `c1fa640`, which is before the deadline. The team considers the first-commit requirement fulfilled by this commit.
- The organisers have not yet confirmed this reading (see the question below).

## Interpretation to confirm with the organisers

The rule text asks that "the first commit" contain at least 1/5 of the final code. Reading it as "the repository reached 1/5 by the first deadline" (which `814ddcf` does, on 4 Oct) is plausible, but it is not the literal wording. This should be confirmed with the organisers when the repository link is sent.

## Deadlines

| Requirement | Deadline | Commit that meets it |
|---|---|---|
| First commit with ≥ 1/5 of code | 8 Oct 2026 | `814ddcf` (4 Oct 2026, 33.4 %). Note: `f3a9770` is the first commit in the history and contains no code. |
| Second commit | 8 Nov 2026 | `06f5685` (5 Nov 2026) |
| Third commit | 24 Nov 2026 | `c1fa640` (24 Nov 2026) |

The rules note that changes after the third commit may not be scored, so documentation must be complete in the repository by that commit.

## Question to the organisers (pending)

Status: **not sent yet, no answer yet.** Send this to the organisers and paste their answer below.

> Subject: WRO 2026 Future Engineers, team X67: two questions on the repository requirements
>
> Dear WRO organisers,
>
> We are team X67 (Colegio Americano de Guayaquil, Ecuador). Our public repository is https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal
>
> 1. **First commit.** Rule 7 asks that the first commit, not later than two months before the competition, contains at least 1/5 of the final code. The first commit in our history (`f3a9770`, 4 Oct 2026) contains only the engineering journal and the test log. The first commit with code (`814ddcf`, same day) contains 33 % of the final code. Is the commit with at least 1/5 of the code accepted as the first commit, or must the first commit itself contain it?
> Could you also confirm the dates of the International Final? Our deadlines are calculated from 8 December 2026.
>
> Thank you,
> Team X67, Henry Riera Loza

**Answer from the organisers:** pending.
