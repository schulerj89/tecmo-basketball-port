# R3B Current-Main Integration QA — Lineage

This report applies to branch
`codex/r3-player-stats-current-main-integration-qa-luna` in worktree
`C:\Users\joshs\Projects\tecmo-basketball-port-r3-player-stats-current-main-integration-qa-luna`.

The accepted code graph is:

```text
bdc2fbb8f5b8497f4855b80e8834696220036aba  (current-main/base)
                 \
                  91f158456d72537f0a8b6ae032cf0b0ade053493  (reconciled merge)
                 / \
7897871aa1e392f7650203e23d106b9ad9d7fbbf  (R3 source candidate)
                         \
                          20dcf9a4d30f8d4e557ab61df5af8fc34458c82c  (fixture correction)
```

The R3 candidate is signed `Good`, has sole parent
`ed060720a98b790f98591af363a490a0e0816018`, and the merge and correction
checkpoint are signed `Good`. The merge tree is
`94c9c2802fe5a5f40ead49a6b591d13e41b4e30d`. The R3 source path ledger and
zero-overlap R2E collision result are recorded in
`docs/finish-tasks/R3-player-stats-leaders/LINEAGE.md`.

`origin/main` was verified at the base SHA. This QA did not push, mutate main,
rewrite the merge or frozen correction checkpoint, modify orchestration/
control-plane state, or add private inputs. The terminal documentation closure
is a new signed descendant of `20dcf9a...`; its exact final identity is
returned by the final audit.
