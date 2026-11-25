# Evaluation route

A reading route for the judges, in the order that follows the Future Engineers 2026 Documentation Rubric (criteria 1 to 5). Everything here is in this repository; nothing needs a login.

## 1. Check the basics (2 minutes)

| What | Where | What to see |
|---|---|---|
| Repository is public | this page | open it without signing in |
| README in English, over 5000 characters | [README.md](../README.md) | about 128 000 characters |
| Team, challenge and status | [README.md](../README.md) "Start here" and "Summary" | Open and Obstacle status |
| Deadlines and submission | [README.md](../README.md) §14.1 | link sent 7 Nov 2026, commits, hard copy |

## 2. Read the criteria in order

| Criterion | Section to read | Supporting files |
|---|---|---|
| 1. Mobility and mechanical design | §5 (chassis, drive, steering, wheels, iterations) | [models/](../models/README.md), [CHANGELOG.md](../CHANGELOG.md) |
| 2. Power and sensor architecture | §6 (power budget, pin map, sensors and calibration) | [schemes/README.md](../schemes/README.md) |
| 3. Software architecture and obstacle strategy | §7 (modules), §8 (Open state machine), §9 (Obstacle map, vision and laps) | [src/README.md](../src/README.md) |
| 4. Systems thinking and engineering decisions | §10 (constraints, trade-offs, risks, why each value) | [docs/ENGINEERING_JOURNAL.md](ENGINEERING_JOURNAL.md) |
| 5. Reproducibility and GitHub quality | §1 (structure), §11 (testing), §12 (build and upload), §14 (versions) | [docs/TESTS.md](TESTS.md), [docs/COMMITS.md](COMMITS.md) |

## 3. Check the commit history yourself

```bash
git clone https://github.com/henryalejandrorieraloza-sudo/WRO-2026-Future-Engineers-Journal.git
cd WRO-2026-Future-Engineers-Journal

# all commits, oldest first, with dates
git log --reverse --format='%h %ad %an %s' --date=short

# code size of the first commit that reached 1/5 of the final code
git ls-tree -r -l 814ddcf | awk -F'\t' '{split($1,a," "); if ($2 ~ /\.(ino|h|cpp|py|html)$/) s+=a[4]} END {print s " bytes"}'

# code size of the final version (main)
git ls-tree -r -l main | awk -F'\t' '{split($1,a," "); if ($2 ~ /\.(ino|h|cpp|py|html)$/) s+=a[4]} END {print s " bytes"}'

# total commits on main
git rev-list --count main
```

| Rule (General Rules §7) | Deadline | Commit or evidence |
|---|---|---|
| First commit with at least 1/5 of the code | 8 Oct 2026 | `814ddcf` (4 Oct, about 33 % of the final code). The first commit in the history, `f3a9770`, has no code. See [docs/COMMITS.md](COMMITS.md) |
| Second commit | 8 Nov 2026 | `06f5685` (5 Nov) |
| Repository link to the organisers | 17 Nov 2026 | sent on 7 Nov 2026 by the team's coach |
| Third commit, used for scoring | 24 Nov 2026 | the state of `main` on that day (see §14.1) |

## 4. Where the numbers come from

- **Open Challenge:** 6 logged runs in [docs/TESTS.md](TESTS.md), all full three-lap rounds with a correct stop.
- **Obstacle Challenge:** 3 logged runs in [docs/TESTS.md](TESTS.md) (the table is kept at 3 runs).
- **Steering limits:** 85 ± 80 = 5 / 165 in both programs. The protractor stops (10 and 170) are the record of the test.
- **Focal length of the camera:** `FOCAL_PX = 50` at 160 × 120, confirmed with [src/tools/camera_viewer/camera_viewer.html](../src/tools/camera_viewer/camera_viewer.html) on 24 Nov 2026.
- **Not measured yet:** stall current of the motor and servo (calculated from the winding resistance), speed over 1 m, Obstacle edge cases in §9.10, and the parking after the third lap (§9.7).
