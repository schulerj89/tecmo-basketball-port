# R1 TIP Fidelity — Luna lineage and diagnostic ledger

Status: Sol source/ABI/pack/test review is accepted for the first TIP
implementation commit. Clean-commit formal proof and independent QA remain
pending.

## Lineage

- Authoritative Sol review task (separate from both Luna tasks):
  `019fc61e-0f2a-7fb0-a76e-e4676808c959`.
- Research Luna: `019fc89b-05fb-7193-aef0-e483f9306279`, exact title
  `Tecmo R1 TIP Fidelity — Original Claim, Automatic Path, and Timing Research — Luna Max`,
  `gpt-5.6-luna/max`, projectless with null Git fields. Created
  `2026-08-03T17:10:30Z`; completed final `18:25:14Z`; metadata closure
  `18:26:39Z`; unpinned false at `18:33:34.575Z`. It had one transmission
  SyntaxError, `Unexpected identifier 'skip_p2_setup'`, and zero literal bad
  requests and zero mutation.
- Implementation Luna: `019fc8ea-543c-7c11-8cae-64f39cec735f`, exact title
  `Tecmo R1 TIP Fidelity — Native Claim and Handoff Implementation — Luna Max`,
  `gpt-5.6-luna/max`, created `2026-08-03T18:37:08Z`. It is the persistent
  pinned/active worker task, reused for every revision, on branch
  `codex/r1-tip-fidelity-luna` in the worktree below, with base/expected parent
  `222d75cfafa9153db1eb44492bf557f11b1a9091`. It has zero literal bad
  requests, no commit, and no mutation outside the authorized boundary.
- Worker worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna`.
- Branch: `codex/r1-tip-fidelity-luna`.
- Base, expected parent, and merge-base: `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- No commit, merge, rebase, reset, clean, push, or subtask was used.
- The later durable clearance authorized only `live_proof_advance_pretip` in
  `src/tecmo_gameplay_live_proof.c`; that function now routes held P1/Away B
  only during `JUMP_CONTEST` and clears it otherwise. No other live-proof line
  was changed.

The initial inventory confirmed the required worktree, branch, HEAD, merge-base, registry, and clean starting state. Root `AGENTS.md` (1,455 lines, 96,366 bytes) and `PORTING.md` (1,684 lines, 105,238 bytes) were read completely.

## Durable rescope and review chronology

- `f6b0a2a` established the initial durable R1 TIP implementation boundary.
- `99f32dd` added exactly `tools/Run-GameplaySceneTests.ps1` for the strict
  gameplay/pre-tip TPTI-2 size/hash/schema row and its named stale-TPTI-1
  negative.
- The string-only rescope at `2026-08-03T19:43:14Z` authorized only the two
  literal active-label changes in `src/tecmo_cli.c` and `src/tecmo_asset_pack.c`.
- `f951098` authorized exactly
  `src/tecmo_gameplay_live_proof.c::live_proof_advance_pretip` to hold
  P1/Away B only during `JUMP_CONTEST`, clearing it in every other phase. This
  recovered the neutral two-human LIVE fixture without changing production
  behavior.
- `43ab7a7` authorized exactly
  `src/tecmo_flow_test.c::flow_finish_gameplay_pretip` to recompute P1 cancel
  on each iteration of its existing 721-iteration loop from the current
  `JUMP_CONTEST` phase, keep P2 neutral, and call the existing runtime update.
  Both preseason and season callers and all post-handoff assertions remain
  unchanged.
- Sol review repeatedly rejected or corrected stale metadata, false-friend
  source roles, mixed raw/Q8 units, fabricated selector/holder mappings,
  equality/underflow/threshold mistakes, unbounded no-progress fixtures,
  skipped-state fabrication, stale error-vs-frame assertions, and any change
  to the accepted scene arc or 721-update presentation contract. The detailed
  implementation corrections remain in the revision list and evidence labels
  below; no complete original selector, tie-settlement, or TTDT trajectory is
  claimed.

## Revision and review history

1. The TPTI-1 payload was expanded to TPTI-2 with 29 source records, a 512-byte header, a 7,680-byte stored payload, exact source records for the validated actor/automatic/opposing/jump/claim/hook/RNG seams, and a strict same-pack TGJS-2 dependency.
2. Source metadata was corrected to distinguish actor state `$0B` at `$9C7F` from dedicated slot-10/global claim state `$17` at `$A2D2`; the old 13-byte `$985E-$986A` input subspan was kept distinct from the 49-byte `$985E-$988E` opposing path.
3. The ordinary full-payload FNV32/FNV64 contract was retained. Embedded mechanics hashes are separately named and do not replace the repository-wide asset identity. The exact-source base was moved to `7008`, and the complete `6656..7007` gap is validated as zero padding.
4. Capture/error, strict automatic threshold, underflow-safe claim comparison, equality deferral, per-jumper commit/altitude diagnostics, bounded contest sampling, and transactional validation were corrected. The visible scene-owned 60-update arc and frame-721 presentation contract remain separate from the incomplete original TTDT trajectory mapping.
5. Scene possession now derives from the resolved logical claimant team and maps through the accepted holder slots `0/5`; opaque raw selectors are retained only as diagnostics. CPU-vs-CPU keeps the accepted native compatibility outcome of Away possession/holder `0` without claiming selector ownership proof.
6. Test helpers were made bounded and completion-aware, no-input stalls were made deterministic and invariant-valid, stale skip-PRETIP fixtures were normalized through the real state API, automatic sample errors were separated from sample frames, and the phase-30 presentation assertion was corrected to FLIGHT.
7. The eight known ball-only checkpoint hashes were updated only after two deterministic base/current comparisons. Unaffected checkpoint hashes remained unchanged. The new proof script derives the home logical sample frame as `ProofFirstFrame + HomeAutomaticSampleFrame + 1` and asserts the resulting value `683`.
8. Active contract strings were changed to TPTI-2 in the owned source/tool surfaces, with TPTI-1 retained only in explicitly named stale/rejected metadata negatives. The two master-authorized literal compatibility edits were limited to the CLI help text and asset-writer failure text.
9. After durable master clearance, the single live-proof fixture seam was
   corrected without changing production behavior: P1/Away held-B is supplied
   only while the current PRETIP phase is `JUMP_CONTEST`; the P2 frame and all
   non-contest phases remain neutral. The warning-clean rebuild, focused CLI
   trio, and broad scene wrapper then passed.
10. After durable master clearance `43ab7a7`, the flow-test fixture seam was
   corrected without changing production behavior: within the existing
   721-iteration `flow_finish_gameplay_pretip` loop, P1 cancel is recomputed
   from the current `JUMP_CONTEST` phase on every iteration, P2 remains neutral,
   and the existing runtime update and both caller/post-handoff assertion paths
   remain intact. The warning-clean rebuild, Win32 launch smoke, focused pre-tip
   harness, and broad scene wrapper passed afterward.

## Fault and recovery ledger

All entries below were read-only or failed before mutation unless explicitly noted. Literal `{detail: bad request}` count is `0` for the complete ledger.

### Initial inventory

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 | `rg : rg: docs/finish-tasks/R1-tip-fidelity: The system cannot find the file specified. (os error 2)` followed by `FullyQualifiedErrorId : NativeCommandError`; symbol/docs inventory | The documentation directory did not yet exist. Inventory continued with existing paths; the directory was later created through the authorized patch. | No mutation or state impact. |
| 1 command, 92 conversion messages and 91 complete `InvalidCastIConvertible` identifiers before truncation | `Cannot convert value "36342607916040656" to type "System.UInt32". Error: "Value was either too large or too small for a UInt32."`; initial FNV inventory | Windows PowerShell multiplied before masking to 32 bits. Recovery used BigInteger/modular arithmetic. | No mutation or state impact. |
| 2 | Symbol inventory truncation warnings; FNV inventory truncation warnings | Output-size limits only; narrower explicit probes were used. | No mutation. |

### Sol read-only evidence diagnostics

| Count | Raw signature / purpose | Recovery | State impact |
|---:|---|---|---|
| 1,283 | `Cannot convert value "-1" to type "System.UInt64"` / `InvalidCastIConvertible`, one per byte across 11 requested ROM spans | Replaced the malformed span probe with one-byte FNV test `E40C292C`, BigInteger modulo `2^32`, and BitConverter hex. | No files or external state changed. |
| 12 | `[System.Convert] does not contain a method named 'ToHexString'` / `MethodNotFound`, whole ROM plus 11 spans | Same corrected read-only probe. Exact ROM SHA and all 11 accepted span FNV32/SHA256 values were verified. | No mutation. |
| 1 | `The system cannot find the file specified. (os error 2)` from nonexistent `decomp\lifted\bank07` probe | Used the actual flat fixed-bank/private disassembly evidence. | No mutation. |
| 1 | Windows-invalid wildcard `src/asset_pack/tecmo_asset_pack_gameplay_pretip.*`, os error `123` | Re-ran with explicit filenames. | No mutation. |

The following Sol-side review diagnostics complete the separate review ledger;
the repeated decomp-path and wildcard facts above remain single-count entries.

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 | `You must provide a value expression following the '+' operator.` / PowerShell `ParserError` | A malformed shell-quoting probe was retried with a valid expression. | No mutation; literal bad-request count remained `0`. |
| 1 | `Cannot convert value "-6545065755768355382" to type "System.UInt64". Error: "Value was either too large or too small for a UInt64."` followed by `FullyQualifiedErrorId InvalidCastIConvertible` | Windows PowerShell 5.1 evaluated the FNV64 hex literal through a signed numeric conversion. Recovery used `[Convert]::ToUInt64("A52B415F53DA85CA", 16)`. | No mutation or external state impact. |
| 1 | `build\\tecmo.assetpack` `ItemNotFound` | Sol's read-only probe targeted a missing build artifact. Recovery used the actual worker pack/build path. | No mutation. |

The missing `decomp\\lifted\\bank07` probe and the Windows-invalid asset-pack
wildcard are recorded once in the preceding Sol read-only table with their
exact `os error 2` and `os error 123` signatures.

### Review-run and output diagnostics

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 | Initial broad/Win32 gameplay smoke marker: `LIVE proof event 'live-handoff' repeat 1 failed` / `real PRETIP-to-LIVE advancement failed` | The neutral two-human fixture drove PRETIP to the fail-closed frame-`720` stall. Recovery was durable clearance `f951098` for the live-proof fixture, followed by `43ab7a7` for the flow-test fixture's phase-routed P1 cancel; the later warning-clean smoke and broad run passed. | No production mutation; fixture-only corrections. |
| 1 | `read_thread` output truncation: `Warning: truncated output (original token count: 139932) Total output lines: 1` | A large historical task read exceeded the output limit. Recovery used compact/direct reads. | No repository or external-state mutation. |
| 1 | `read_thread received invalid arguments: turnLimit: Too big: expected number to be <=10.` | A read-only inspection requested 30 turns. Recovery used `turnLimit=10`. | No repository or external-state mutation. |

### Implementation-turn diagnostics

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 2 | `Failed to build asset pack: TPTI-2 declared source containment contract rejected.` | Early source records pointed at dedicated offsets rather than the intentional closeup/setup overlaps. Offsets were corrected. | No tracked/external mutation; later pack build passed. |
| 1 | `Unable to find type [BigInteger].` | The inline FNV helper used an unqualified PowerShell type. Recovered with `[System.Numerics.BigInteger]`. | No state impact. |
| 1 | `Failed to build asset pack: TPTI-2 ordinary full-payload fingerprint mismatch (got 28910BC1/7EA1596E8DFAC0C1).` | `exact_source_base` changed while temporary constants were stale. Recomputed bootstrap and restored final constants. | No mutation. |
| 1 | `apply_patch verification failed: Failed to find expected lines ... tecmo_gameplay_pretip.c ... away_tip_automatic != true` | Combined altitude/test patch used stale context. Recovered with smaller exact patches. | No mutation. |
| 1 | `apply_patch verification failed: Failed to find expected lines ... tecmo_gameplay_pretip.c` followed by the wrapped late-auto loop block | Same stale-context issue. Recovered with smaller exact patches. | No mutation. |
| 1 | Revision-150 `apply_patch verification failed: Failed to find expected lines in C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna\src\tecmo_gameplay_scene_test_pretip.c:` followed by the automatic-both expectation block with the old error/frame comparisons | The block had already changed in an earlier patch; the exact combined hunk had stale context. Recovered by splitting the edit. | No mutation. |
| 1 | `apply_patch verification failed: Failed to find expected lines ... tecmo_gameplay_scene.c` followed by the long scene status string | Context had moved after the fixture edit. Recovered with explicit contexts. | No mutation. |
| 1 | `rg: src/tecmo_gameplay_scene_test*.c: The filename, directory name or volume label syntax is incorrect. (os error 123)` | Windows wildcard inventory. Recovered with explicit filenames. | No mutation. |
| 1 | `rg: regex parse error: (?:TPTI-1|TPTI-2|5888|99ADFE3D|256|< 20|\\[192)` / `error: unclosed character class` | Malformed audit regex. Recovered with simpler fixed/explicit searches. | No mutation. |
| 1 | `rg: regex parse error: (?:\\[192|< 20|TPTI-2)` / `error: invalid character class range, the start must be <= the end` | Malformed audit regex. Recovered with simpler searches. | No mutation. |
| 1 | `rg: src/tecmo_gameplay_scene*.h: The filename, directory name or volume label syntax is incorrect. (os error 123)` | Windows wildcard header inventory. Recovered with explicit filenames. | No mutation. |
| 1 | `apply_patch verification failed: invalid hunk ... is empty` for `src/tecmo_gameplay_pretip.c` | Empty/malformed hunk. Recovered with a nonempty exact patch. | No mutation. |
| 1 | `apply_patch verification failed: invalid hunk ... is empty` for `src/tecmo_gameplay_scene_test_pretip.c` | Empty/malformed hunk. Recovered with a nonempty exact patch. | No mutation. |
| 1 | `At line:2 char:20 ... The token '&&' is not a valid statement separator in this version.` | Bash separator used in Windows PowerShell status/diff command. Recovered with separate statements. | No mutation. |
| 1 | `CreateProcess ... The directory name is invalid. (os error 267)` | Mistyped worktree path omitted `-tip` during rebuild. Recovered with the exact worker path. | No mutation. |
| 3 | `Gameplay pre-tip test failed: PACK path required`; `Gameplay pre-tip human checkpoint failed: PACK path required`; `Gameplay scene test failed: PACK path required` | Focused executable probes initially omitted the asset-pack argument. Recovered by passing `build\tecmo.assetpack`. | No mutation. |
| 1 | `rg: README*: The filename, directory name or volume label syntax is incorrect. (os error 123)` | Windows-invalid wildcard. Recovered with explicit source paths. | No mutation. |
| 1 | `TPTI-2 render checkpoint 'gameplay-pretip-frame680' changed: A707A2C6D82DD2E2B2BAC3B5ABC7F790A95E13CC5BC825EEDEA330A5A9C1445C` | Expected result changed only for the accepted no-snap ball sprite. Focused visual comparison later confirmed the bounded 136-pixel ball-only diff. | Ignored scratch only; no tracked mutation. |
| 8 | `if : The term 'if' is not recognized as the name of a cmdlet...` from temporary compact visual-ledger formatter | Inline PowerShell expression was parsed as a command. Recovered with explicit `$bbox` assignment. | No file/state mutation. |

### Build and external-run ledger

- `build.ps1` succeeded repeatedly after the corrections. Final output contained no compiler warning lines. The existing build script also updated `C:\Users\joshs\OneDrive\Desktop\Tecmo Basketball Native Port.lnk`; this side effect is recorded and was not independently edited.
- One intermediate build warning was emitted: `src/tecmo_gameplay_scene_test_pretip.c(671) : warning C4700: uninitialized local variable 'winner' used`. It came from temporary debug output before assignment; debug output was removed and subsequent builds were warning-clean.
- Asset-pack creation succeeded after recovery: `Wrote 8 PRG banks, 32 CHR banks, 86 entries...`.
- Sol independently ran the earlier three CLI checks without mutation: pre-tip
  test PASS, human checkpoint PASS, and scene test failed at the stale
  `pre-tip 721-frame track-8-to-5 handoff` fixture. After the durable fixture
  clearance, the worker rebuilt and ran the broad wrapper successfully:
  `GAMEPLAY SCENE TEST PASS` and `LIVE PROOF DRAFT`, root
  `build\live-proof-20260803T204206008Z`, manifest SHA256
  `BD3DF1C9D85BD82A883DEED0FF23AC6C6EC17F66E12C8DFEEA2773C8D9DB0938`.
- The worker's post-`43ab7a7` rebuild used `TECMO_SKIP_SHORTCUT=1` to suppress
  unrelated shortcut side effects, exited `0`, and emitted no warning lines.
  The subsequent read-only Win32 launch smoke exited `0` and reported:
  `Win32 shortcut launch smoke test passed: GUI/console subsystems,
  project-root arguments, working directory, icon, roster-independent
  startup, window lifetime, and clean shutdown verified.`
- The same post-clearance focused harness exited `0` with the complete TPTI-2
  source-role/dependency/mutation/deterministic-render/scene-integration PASS
  ledger. The broad worker run exited `0` with `GAMEPLAY SCENE TEST PASS` and
  DRAFT root
  `build\live-proof-20260803T205139679Z`, `14` frames, `2` native videos,
  and a `1920x1440` contact sheet.
- Sol's independent broad rerun also exited `0`:
  `Run-GameplaySceneTests.ps1 -Build`, `GAMEPLAY SCENE TEST PASS`, DRAFT root
  `build/live-proof-20260803T204552541Z`, `14` frames, `2` videos, and a
  `1920x1440` contact sheet. This is recorded separately from the worker run.
- Sol's latest personal post-flow gates all exited `0`: `build.ps1` built the
  console and Win32 targets with no warning lines;
  `Run-Win32LaunchSmokeTest.ps1` passed explicit console flow and GUI/console
  production smoke; `Run-GameplayPreTipTests.ps1` passed; and
  `Run-GameplaySceneTests.ps1 -Build` passed with LIVE PROOF DRAFT at
  `build/live-proof-20260803T205847090Z`. This is the accepted source/ABI/
  pack/test review point for the first implementation commit. Clean-commit
  formal proof and independent QA remain pending.

### Commit-audit diagnostic

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 command, 3 reported lines | Initial `git diff --cached --check` reported `docs/finish-tasks/R1-tip-fidelity/README.md:8: trailing whitespace.`, with the same diagnostic at lines 10 and 11. | Three pre-existing Markdown hard-break spaces were removed from the authorized README; the cached diff check was rerun and passed. | No implementation, repository-state, or external-state impact beyond the authorized documentation cleanup. |
