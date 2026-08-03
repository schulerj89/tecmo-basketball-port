# Independent Luna QA and proof audit

## Allocation

- Thread: `019fc8d6-7a77-79d0-a352-767a228ef478`
- Exact title: `Tecmo R1A CPU+LIVE Integration — Independent QA and Proof Audit — Luna Max`
- Model/thinking: `gpt-5.6-luna` / `max`
- Allocation: projectless/null Git; branch/worktree none; tracked writable scope
  none
- Pin: pinned immediately after creation and retained across every revision
- Reporting boundary: only `S-SOL-R1A-INTEGRATION-QA-001`; no master, other
  Sol/Luna, or subagent contact
- Fault lineage: first creation succeeded; literal/equivalent bad-request 0;
  replacement thread none

## Initial read-only audit

The worker read the complete repository `AGENTS.md` and `PORTING.md`, every
committed CPU/LIVE record, the seven-commit candidate range, and the relevant
production, ABI/build-list, test, source-map, and proof-runner paths. It found
no runtime/source integration defect. Initial open counts were P0 `0`, P1 `4`,
P2 `1`:

1. CPU lifecycle `-RequirePass` records its base but does not enforce ancestry
   or reread final Git state.
2. The LIVE runner pins its original feature branch/base rather than exposing
   an R1A provenance mode.
3. The LIVE importer checks original frames/contacts but not the supplied CPU
   manifest's full schema/task/status/ROM/FCEUX/Git/asset identity.
4. Focused/scene wrappers recursively clear fixed ignored scratch roots after
   containment checks but without an ownership marker.
5. Accepted LIVE handoff docs retain historical pending-verification/merge
   wording.

These observations are preserved; the frozen source/test/tool slice was not
edited to hide them.

## Same-worker control and proof disposition

The Sol personally reviewed the findings and returned the exact compensating
controls and immutable artifacts to the same worker. The worker independently:

- verified the 577082-byte frozen proof manifest SHA-256
  `8E245A62676832F2D75E7BD930682CEA1ED51D80064860B90178691E5251F0C4`;
- matched every one of its 254 inventory entries, 189 logs, 14 frames, and 13
  negative gates; independently probed/decoded both videos and compared all
  repeat event/state invariants;
- fully validated the 128484-byte CPU manifest SHA-256
  `E7C9E6C9210D398DADC82715779A1389DF881643D109A0FDB091EBAFA523254A`,
  including schema/task/status, Rev1 ROM, FCEUX, Git, TGAI, source/map/runner,
  both original runs, and all listed artifacts;
- proved the CPU manifest's `8be7a9f...` terminal through `ad0f005...` is a
  docs-only successor with unchanged source/script identities;
- proved each fixed scratch path absent and contained before running, then
  absent after running, in the exclusive worktree; and
- independently passed a warning-clean redirected build, CPU `680/24/17`,
  movement, production flow, season, and redirected Win32 smoke.

Disposition after exact controls: evidence-invalidating P0 `0`, P1 `0`, P2
`0`. Findings 1–4 remain reusable proof-harness hardening, not invalid current
evidence. Finding 5 is historical language closed by this later R1A record
without rewriting the frozen LIVE docs. No bounded evidence-repair task was
required.

The worker also correctly separated temporal state: the frozen proof ended at
`18:21:09Z` while `main/origin/main=6d8f9c7...`; accepted R3A later moved both
refs to `dd096cb...`. That post-proof move is not represented as proof-time
state.

## Intermediate merged-tip review

Master authorized reconciling accepted R3A into the R1A branch. The Sol made
merge `f98fea320bf2340e0c6c9b226cfe6caa63196dd7`, reran the full personal gate,
and generated fresh ignored proof
`build\r1a-integration-qa-merged-20260803-d`, whose 576034-byte manifest
SHA-256 is
`99BAE86564963189A5A93B4975BA8B791AB2CF2190E964BD9932004BE770F747`.

The same pinned worker received the exact merge parents, current refs, merged
proof root/hash, full gate results, and this draft. During that review it
correctly discovered a later accepted R4A advance:
`main=origin/main=bcacd5b6963f4db1a92c8db9b9770413505a0e98`. Because neither
`f98fea3...` nor `bcacd5b...` contained the other, it stopped without terminal
acceptance and identified the resulting stale report lines as provisional P2,
not evidence-invalidating. P0 `0`, P1 `0`, provisional P2 `1`.

Master then explicitly authorized the required current-main reconciliation.
The Sol proved 43 R1A-side paths versus 18 R4A-side paths with overlap 0,
obtained Git-native merge-tree exit 0, and made
`351f446dddc96c34c838c5a9642a0be9d7f1411e` with ordered parents `f98fea3...`
and `bcacd5b...`. It reran the complete R1A gate plus the accepted R4A music,
frontend-audio, and gameplay-audio gates. Fresh proof
`build\r1a-integration-qa-final-20260803-f` has a 575510-byte manifest,
SHA-256 `F72DC259DFFD6B95B560088D591A059DE9F572ED30D2FA70E889A32977273F43`,
and current/final `351f446...`.

## Terminal same-worker review

The same pinned worker received the exact terminal merge/proof/gates and the
corrected five-file draft. Final disposition: **P0/P1/P2 `0/0/0`**. It
independently confirmed the terminal Git/ref/status/path scope, 43/18/0 seam,
complete proof inventory, media probes and decoded sidecars, paired event/state
invariants, external CPU manifest/artifacts, and corrected future-tense lineage
paragraph. True fast-forward from `bcacd5b...` remains valid while refs remain
exact; no evidence-repair or boundary task is required.

Its transient wrong-ref-pair, decoded-sidecar-path, PowerShell/validator/parser,
and output-truncation diagnostics were recovered without evidence impact.
Literal/equivalent bad-request count remained `0`.

## Hardening backlog (outside this frozen lane)

Future master-routed work may add exact-terminal/base/final Git enforcement to
the CPU proof runner, a first-class R1A branch/provenance mode to the LIVE
runner, full CPU-manifest identity validation to the importer, and unique
ownership-marked scratch roots. These improvements are not claimed complete by
R1A and are not prerequisites for the specifically controlled evidence here.
