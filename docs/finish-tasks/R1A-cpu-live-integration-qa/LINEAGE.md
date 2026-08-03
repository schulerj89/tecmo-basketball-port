# Git, control, diagnostic, and acceptance lineage

## Candidate ancestry

The common base and frozen CPU+LIVE chain are:

```text
6d8f9c7a99a7ce188f1a523247d3a9b9093860fb  common base / proof-time main
db5a043244361b3e9bbab2e154c7f14e4a4a5014  CPU lifecycle implementation
8be7a9f9a11d43e68b090a98af122758885931fd  CPU proof-time terminal
a2f0238f7380a1cc9bc16d46bf54ddd17a69e721  CPU formal-proof docs closure
ad0f005673692b04772bce3c3b4d3ac4b2624731  CPU accepted docs terminal
e2333db8fd0ad21c036d0016574c1551929fbb5c  LIVE implementation/proof
6a16422b02e6354bfaf67f731e7a0e5b05906a17  LIVE formal proof PASS
222d75cfafa9153db1eb44492bf557f11b1a9091  frozen CPU+LIVE terminal QA
```

After the frozen proof, master advanced `main` and `origin/main` through
accepted R3A to `dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d`. The common merge base
between `222d75cf...` and `dd096cb...` is `6d8f9c7...`. The sides changed 43
and 9 paths respectively with zero path overlap and no merge-tree conflict.

Master explicitly authorized a current-main reconciliation in this registered
Sol branch. The branch-only no-fast-forward merge is:

```text
f98fea320bf2340e0c6c9b226cfe6caa63196dd7
  parent 1: 222d75cfafa9153db1eb44492bf557f11b1a9091
  parent 2: dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d
```

Both parents are verified ancestors. Accepted R4A then advanced `main` and
`origin/main` to `bcacd5b6963f4db1a92c8db9b9770413505a0e98`. Neither that tip nor
`f98fea3...` contained the other; their merge-base was `dd096cb...`.

Master explicitly authorized the required second reconciliation. The R1A side
changed 43 paths from `dd096cb...`, R4A changed 18, normalized overlap was 0,
and Git-native merge-tree returned exit 0 with predicted tree
`2bc6641c19abad1266fd7a8b1f3d2ea1b28922ee`. The branch-only merge is:

```text
351f446dddc96c34c838c5a9642a0be9d7f1411e
  parent 1: f98fea320bf2340e0c6c9b226cfe6caa63196dd7
  parent 2: bcacd5b6963f4db1a92c8db9b9770413505a0e98
  tree:     2bc6641c19abad1266fd7a8b1f3d2ea1b28922ee
```

R1A, R3A, and R4A are all verified ancestors. `main` and `origin/main` remained
`bcacd5b...`; this lane never checked out, committed on, pushed, or otherwise
mutated `main`.

After the terminal Luna confirmation, the final signed report commit will have
`351f446...` as its sole parent and will change exactly five tracked files
under `docs/finish-tasks/R1A-cpu-live-integration-qa/**`. Its exact SHA will be
reported in the terminal master handoff rather than through a self-referential
edit.

## Temporal refs

| Observation | `main` | `origin/main` | R1A HEAD |
|---|---|---|---|
| clean takeover and frozen proof | `6d8f9c7...` | `6d8f9c7...` | `222d75cf...` |
| later accepted R3A advance | `dd096cb...` | `dd096cb...` | `222d75cf...` |
| after authorized branch-only merge | `dd096cb...` | `dd096cb...` | `f98fea3...` |
| later accepted R4A advance | `bcacd5b...` | `bcacd5b...` | `f98fea3...` |
| after required second reconciliation | `bcacd5b...` | `bcacd5b...` | `351f446...` |
| final docs-only handoff | `bcacd5b...` verified at handoff | `bcacd5b...` verified at handoff | exact report SHA in handoff |

## Control checkpoints

- Master registered `S-SOL-R1A-INTEGRATION-QA-001` at control-plane commit
  `ae4b71b` and accepted the clean takeover.
- Master collision-cleared exactly one projectless Luna Max. Its creation,
  immediate pin, null-Git allocation, no-fault lineage, initial findings,
  exact-control disposition, and terminal review were checkpointed.
- Master was notified when unrelated accepted R3A moved main, then explicitly
  authorized reconciliation at the current main tip.
- When accepted R4A advanced main after the first proof, both the Sol and same
  Luna reported the divergence. Master explicitly authorized the second
  reconciliation; no ref movement was silently normalized.
- The Sol reported zero overlap at both seams, exact merge parents, fresh gates,
  proof hashes, and terminal status. No product fix or conflict improvisation
  was performed.

## Diagnostics and recovery

All diagnostics were recovered and are not hidden product failures:

| Diagnostic | Count | Recovery/result |
|---|---:|---|
| Personal PowerShell inline-parenthesis parse errors in read-only Git probes | 2 | Split assignments; no mutation |
| Thread listing unsupported `limit=100` | 1 | Reissued with supported limit |
| Windows `rg` wildcard path errors | 2 | Reissued against directories |
| Instruction/doc output truncation | 1 | Re-read every file completely in bounded chunks |
| Initial no-pack flow/intro reached expected fail-closed boundary | 2 sequences | Built canonical ignored pack; production reruns PASS |
| Unadapted LIVE RequirePass branch rejected and left root `-a` empty | 1 | Exact-one-match in-memory adaptation on fresh accepted roots |
| Initial `.Split` branch-count and negative-field projection assumptions | 2 | Corrected inspection logic; no file mutation |
| Independent worker PowerShell/Git encoding, unsupported option, helper alias, interpolation/brace/parenthesis diagnostics | recovered set | Split into smaller probes; all terminal validators PASS |
| Independent worker dimension/event-order validator assumptions | 2 | Corrected false positives; underlying hashes already matched |
| Independent redirected Win32 icon-path adaptation mismatch | 1 | Used matching ignored-root icon; rerun PASS |
| Post-proof `main` differed from proof-time expectation | 1 | Time-scoped refs; master-authorized zero-conflict merge and full rerun |
| `ffmpeg`/`ffprobe -version` piped to early-closing `Select-Object` returned nonzero | 1 | Tool paths were valid; independent full probe/decode audit PASS |
| First batched UI view partially rendered the second identical contact | 1 | Reopened individually; full image correct and byte/hash identity independently proven |
| Text scan of merge-tree output matched the word `conflict` inside an added documentation sentence | 1 | Replaced heuristic with Git-native `merge-tree --write-tree --messages`; exit 0 and predicted tree verified |
| Unquoted PowerShell `HEAD^{tree}` probe was command-encoded incorrectly | 1 | Reissued with quoted revision; exact merge/tree/ancestry verified |
| Compressed final event-audit function calls omitted required spaces | 1 | Reissued with explicit `Get-State` calls; all event invariants PASS |
| Initial staged diff-check found Markdown hard-break trailing spaces in the signature block | 1 | Converted the block to bullets; rerun PASS |

Literal/equivalent bad-request session-fault count: `0` for the Sol and `0`
for the Luna. No replacement or fault-lineage branch was required.

## Draft cleanliness control

RequirePass needed a clean nonignored worktree while the five authorized draft
reports were untracked. The Sol resolved and containment-checked the exact
source and task-owned destination, required the destination absent, hashed all
five drafts, moved the directory temporarily, ran the proof, and restored it.
All five pre/post SHA-256 values matched exactly. No tracked file moved, and
Git was clean during the proof.

## Scope and ownership audit

- Frozen CPU+LIVE product/source/assets/tests/scripts at `222d75cf...` were not
  manually edited by this lane.
- Accepted R3A and R4A arrived only through authorized merge parents
  `dd096cb...` and `bcacd5b...`; no conflict resolution or product edit was
  made.
- Manual tracked writes are exactly five report files under the registered
  writable glob.
- Asset packs, executables, frames, contacts, videos, logs, manifests, JSONL,
  shortcuts, and test reports remain ignored under `build/`.
- No ROM, decompilation, ASM, private emulator capture, save state, or other
  proprietary artifact is committed.
- Final checks enforce report-parent identity, both-parent ancestry, exact
  allowed-path count, `git diff --check`, clean index/worktree, and current
  `main`/`origin/main` identity.

## Commit/signing policy

The report is personally signed in `README.md` and committed with Git
`--signoff`, producing a `Signed-off-by: Josh Schuler <jaystar524@gmail.com>`
trailer. Because `bcacd5b...` is already a parent of the report lineage, master
may fast-forward from that exact ref to the terminal report commit with a
non-force merge. If `main` advances again before integration, master must
re-evaluate rather than applying the recorded instruction blindly.
