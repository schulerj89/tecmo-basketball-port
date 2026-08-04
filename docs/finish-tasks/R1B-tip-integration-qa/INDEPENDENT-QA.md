# Independent Luna QA

## Allocation

- Thread: `019fca13-3e2c-7e52-9e7a-198c66bdfef8`
- Title: `Tecmo R1B TIP Integration Independent QA — Luna Max`
- Model/thinking: `gpt-5.6-luna` / `max`
- Topology: one separate top-level projectless task; null Git allocation;
  read-only against the assigned Sol worktree
- Tracked writable scope: none
- Pin: pinned immediately after creation, retained for the initial reviews,
  unpinned after the first terminal PASS, then re-pinned and reused for the
  live-main reconciliation review
- Reporting boundary: only this R1B Sol orchestrator; no master, other
  Sol/Luna, or subagent contact
- Creation/replacement lineage: attempt 1; literal/equivalent bad-request
  count `0`; replacement none

## Initial read-only audit

The worker received the exact worktree, branch, merge SHA/tree/parents,
candidate and current-main SHAs, reservation checkpoint, allowed scope,
forbidden actions, personal gate results, proof root/hashes, ROM identity, and
the full additional ASM/native traceability gate.

It independently inspected the instructions, accepted R1 TIP reports, merge
range, candidate path set, product/runtime/parser/source-map/test/proof code,
proof manifest/media inventory, and canonical ROM spans. No product/runtime
defect or P0/P1 was reported. Its formal initial disposition was
**BLOCKED — P0/P1/P2 `0/0/1`** for the stale equal-Away guidance. It also
surfaced the source-map/report traceability boundary that the required terminal
table needed to make explicit:

1. The committed TPTI-2 slot-10 source-map string pins
   `$A214-$A2DE`, equality, `$A2D2`, and `$A2D5`, but does not spell out
   raw subdispatches `$1A->$A25F` and `$1B->$A274`.
2. `AGENTS.md` and `PORTING.md` said equal errors choose Away, while
   `src/tecmo_gameplay_pretip.c::tip_try_resolve_claim`, its self-test, and
   all accepted R1 TIP records defer equality because the original tie policy
   remains incomplete.

The Sol independently confirmed both findings. Finding 1 is resolved within
the owned traceability table in [EVIDENCE.md](EVIDENCE.md), explicitly without
claiming the committed source-map string changed. Finding 2 was reported to
master before acceptance; master collision-cleared and signed the narrow
rescope at `9a3b4623022e3e4dc46142f5370f26c705bd9fe3`. Good-SSH-signed
guidance-only commit `7ba0066ca1084e971a268d0b1b0176d065fdbd01`
changes only `AGENTS.md` and `PORTING.md`.

The initial audit independently confirmed:

- exact branch, merge SHA/tree/ordered parents, signature chain, ancestry,
  56-path current-main side, 24-path candidate side, overlap 0, and exact
  24-path merge delta;
- all requested canonical ROM span FNV32/FNV64 values and full ROM SHA/FNV;
- TPTI-2's 29 source roles, six same-pack dependencies, strict TGJS-2 identity,
  bounds/overlap/padding and malformed-input gates;
- both 65-frame render passes, manifest and frame hashes, 138 clean logs,
  ffprobe dimensions/count/rate, and representative full-resolution media;
- focused comparison
  `23EA2D0980E6F37F059B65C1549EB9602433AE082D1E0A8861D6A59C68F6E479`;
  and
- the exact/native-approximate/incomplete boundary without replacing Sol
  visual acceptance.

## Initial worker fault/retry lineage

Every initial-turn diagnostic was read-only and recovered:

| Diagnostic | Recovery |
|---|---|
| Large instruction/report output truncated | Re-read bounded chunks to EOF |
| Inline PowerShell `git diff` status expression parse error | Split into separate commands |
| Unquoted `^{tree}` parsed by PowerShell | Quoted the revision; exact tree obtained |
| One mistyped commit argument | Reissued with exact parent |
| JavaScript wrapper contained a PowerShell continuation character | Rebuilt the read-only probe safely |
| Initial source-map path omitted `src/asset_pack` | Reissued against the correct path |
| Manifest probe assumed wrong fields/nonexistent `frame_records` | Inspected schema; used `repository`, `render`, and `frames` |
| First UInt32 FNV implementation overflowed | Reissued with BigInteger arithmetic |
| BigInteger format added sign padding | Normalized leading zeros; all requested spans matched |
| Raw-vector wrapper interpolation error | Reissued using PowerShell format strings |
| Byte-typed vector shift yielded `$005F/$0074` | Cast to integers; obtained `$A25F/$A274` |
| Guessed facing-log filename absent | Used existing pass1/pass2 names |

No command modified a file, artifact, ref, branch, index, or worktree.
Literal/equivalent bad-request count remained `0`.

## Same-worker terminal review and revision

The same pinned worker reviewed Good-SSH guidance commit `7ba0066...` and the
actual five-report snapshot. Its first terminal-report disposition was
**BLOCKED — P0/P1/P2 `0/0/2`**. It independently confirmed:

- both authorized guidance files now defer equal claims and preserve incomplete
  original tie settlement, with no runtime/source-map edit;
- all ten traceability rows are substantively correct, including the raw
  `$1A->$A25F/$1B->$A274` dispatches and the explicit statement that the
  committed source-map string was not upgraded;
- every documented canonical ROM/source FNV32/FNV64 value, TPTI-2 metadata,
  same-pack TGJS-2 dependency, parser gate, and false-friend boundary;
- `$E537/$E542` ordering, strictly limited mapper-gated `$E56E`,
  frame-721 non-ROM-exact status, and every incomplete ownership/tie/TTDT/
  trajectory/close-up boundary; and
- 65/65 deterministic proof frames with mismatch 0 and the focused comparison
  SHA-256
  `23EA2D0980E6F37F059B65C1549EB9602433AE082D1E0A8861D6A59C68F6E479`.

Its two report-only P2 findings and exact Sol resolutions were:

1. `EVIDENCE.md` named the stale/non-authoritative short Bank05
   `$985B-$988E` derivative. The report now names only authoritative
   `C-0111_bank05_large_state_and_trajectory_cluster_985B_BFA7.asm` and
   explicitly rejects the stale derivative as source of truth.
2. The reports still described their terminal disposition/retry record as
   future work. This section now records the actual blocked result, both exact
   remediations, and the complete new-turn retry lineage.

New-turn read-only diagnostics:

| Diagnostic | Recovery |
|---|---|
| Draft initially used `$DecompRoot/lifted` | Resolved and rechecked `$DecompRoot/decomp/lifted` |
| Wrapper interpolation syntax error in first ROM-hash probe | No command ran; used non-interpolating PowerShell formatting |
| Additional wrapper/PowerShell parse errors | Split and simplified the read-only probes |
| Unquoted `^{tree}` | Quoted the revision and verified the tree |
| Wrong manifest path/schema assumptions | Inspected the actual manifest and field layout |
| Proof probe assumed nonexistent pass1/pass2 directories | Resolved the real first-pass root and `determinism-pass-2` |
| Supplemental facing image was paired with frame 661 | Filtered exact `tipoff-*.png`; 65/65, mismatch 0 |
| Hash display omitted leading zeros | Normalized widths; all twelve documented span hashes matched |
| Large outputs truncated | Re-read bounded chunks |

No worker command modified a file, artifact, ref, branch, index, or worktree.
Literal/equivalent bad-request count remained `0`; replacement task none.
After the two exact report corrections, the Sol's open disposition is
**P0/P1/P2 `0/0/0`**.

## Final exact-fix confirmation

The same still-pinned task re-read the authoritative-source correction,
closure history, guidance-only diff, five-report snapshot, and underlying
source/proof seams. Final verdict: **PASS — P0/P1/P2 `0/0/0`**.

It confirmed that:

- only authoritative
  `C-0111_bank05_large_state_and_trajectory_cluster_985B_BFA7.asm` is named
  as accepted source, with the stale short derivative explicitly rejected;
- the prior `0/0/2`, both exact remediations, and complete retry history are
  recorded consistently across this file, `README.md`, and `LINEAGE.md`;
- signatures, merge tree, two-file guidance scope, and exactly five authorized
  untracked report files remained exact;
- raw slot-10 dispatches, same-pack TGJS-2, `$E537/$E542`, limited `$E56E`,
  both false friends, and every native-approximate/incomplete boundary remained
  accurate; and
- frame 721 remains native-approximate and is not promoted to ROM-exact.

Closure-turn diagnostics were also read-only and recovered:

| Diagnostic | Recovery |
|---|---|
| Unquoted PowerShell `HEAD^{tree}` | Quoted the revision; expected tree matched |
| Broad output | Re-read in bounded ranges |
| Porcelain-v2 filter used the wrong untracked prefix | Corrected the filter; exactly five authorized drafts, unexpected files 0 |

Literal/equivalent bad-request count remained `0`; no replacement task was
created. The worker modified no file, artifact, ref, branch, index, or worktree.
After the first terminal acceptance, task
`019fca13-3e2c-7e52-9e7a-198c66bdfef8` was unpinned successfully; it was
not archived because branch integration remains master-owned.

## Post-reconciliation terminal review

After accepted main advanced to `8a5b992...`, the same completed task was
re-pinned and given signed reconciliation `3aa7dfb...`, its required ordered
parents/tree, the fresh affected-gate results, the current proof root, and the
revised five-report snapshot. It remained projectless and read-only. Final
verdict: **PASS — P0/P1/P2 `0/0/0`**.

It independently confirmed:

- Good-SSH signatures for the accepted candidate, initial merge, guidance
  correction, accepted advanced main, and reconciliation; exact reconciliation
  parents `7ba0066...` then `8a5b992...`; tree `fb2e4cd...`; live
  `origin/main` `8a5b992...`; normalized overlap `0`; clean tracked state and
  exactly five authorized drafts;
- all five reports to true EOF, the two guidance corrections, authoritative
  long C-0111 source record and stale-short-record rejection, every trace-table
  mapping/classification, raw `$1A->$A25F/$1B->$A274`, strict same-pack
  TGJS-2, parser/provenance gates, both false friends, `$E537/$E542`, limited
  `$E56E`, and all six incomplete boundaries;
- fresh proof commit/schema/ROM/executable/pack identity, 65 first-pass plus 65
  second-pass frames, deterministic mismatch `0`, exact `0661..0725` sequence,
  138 logs, and five accepted-media mismatches `0`; and
- existing asset-pack and sanitized reconciliation reports, including 55/55
  asset vectors and NativeFlow success. The worker only inspected existing
  media; Sol retained product execution and personal visual acceptance.

One actionable report-only P2 was found during this review: the historical
proof command in `COMMANDS.md` used the reconciled `$ProofRoot` variable while
the adjacent hashes correctly described the original `564d838...` proof. Sol
changed that invocation to `$InitialProofRoot`; the same active worker re-read
and verified the exact fix. No runtime, source-map, test, build, or proof-tool
path changed. A provisional second alert that the unresolved-boundary list had
only one item was disproved by a true-EOF reread: all six bullets were already
present, so it is diagnostic lineage rather than a finding.

Complete post-reconciliation worker fault/retry lineage was read-only:

| Diagnostic | Recovery |
|---|---|
| Bounded `EVIDENCE.md` read appeared to stop after one unresolved bullet | Re-read to true EOF; all six bullets were present |
| Broad artifact probe streamed binary image content and truncated | Replaced with extension-filtered bounded inventory |
| Raw-ROM brace/parser error and initial wrong bank-window mapping | No command ran for the parse error; rebuilt the read-only decoder from the decomp manifest and reproduced the canonical pointers/anchors |
| Span formatter used array-expression offsets and unstable unsigned widths | Normalized offsets and fixed-width formatting before relying on results |
| Invalid `git show-ref --remotes` and malformed combined `rev-parse` | Reissued isolated supported ref/object queries |
| Two relative decomp-root probes targeted the port worktree | Used the absolute documented `$DecompRoot/decomp/lifted` path |
| First proof parser used `manifest.json` | Read the actual `proof-manifest.json` |
| First proof comparison was nonrecursive and a broad glob was ambiguous | Used recursive exact four-digit frame filters and explicit five-media names |

The worker modified no file, artifact, ref, branch, index, or worktree.
Literal/equivalent bad-request count remained `0`; replacement task none.
After this final verdict, the same task was unpinned successfully and was not
archived. Main integration remains master-owned.
