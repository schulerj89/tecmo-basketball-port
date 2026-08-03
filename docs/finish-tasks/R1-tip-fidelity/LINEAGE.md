# R1 TIP Fidelity — Luna lineage and diagnostic ledger

Status: Sol accepted source/ABI/pack/test review and clean-commit formal proof
for implementation commit `a37e10207455933be3930e90c55b10b669cb0ef3`.
Independent terminal QA passed the frozen product/proof at
`b678beffeacd745fe438e78d323357dc6f86af95` with `P0=0`, `P1=0`, and one
grouped docs-only `P2`; the same QA task must verify this revised doc tip
before terminal acceptance. Sol-branch integration remains pending.

Accepted chain: `222d75cfafa9153db1eb44492bf557f11b1a9091` ->
`a37e10207455933be3930e90c55b10b669cb0ef3` ->
`b678beffeacd745fe438e78d323357dc6f86af95`. This correction is the terminal
docs-only closure commit; its exact SHA is reported in Git/worker output and
is intentionally not self-embedded here.

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
  `222d75cfafa9153db1eb44492bf557f11b1a9091`. First implementation commit
  `a37e10207455933be3930e90c55b10b669cb0ef3` exists with that exact parent;
  the worker has zero literal bad requests and no mutation outside the
  authorized boundary.
- Worker worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna`.
- Branch: `codex/r1-tip-fidelity-luna`.
- Base, expected parent, and merge-base: `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- No merge, rebase, reset, destructive clean, push, or subtask was used.
- At this pre-closure snapshot, the docs-only closure commit is the current
  pending action; its SHA is intentionally not yet assigned.
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

### Sol-only metadata diagnostic

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 | `SyntaxError: Unexpected identifier 'Tecmo'` | The Sol's first formal-proof/master-checkpoint send payload contained unescaped title delimiters in a JavaScript template literal, so the local `functions.exec` wrapper failed before invoking `codex_app__send_message_to_thread`. Recovery used a quoting-safe plain-string retry, which succeeded to master thread `019fc5d4-f360-78b3-b2a6-c8bae92df690`. | No repository, artifact, worktree, task, or emulator mutation; literal bad-request count remains `0`. |

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
  formal proof passed afterward; independent terminal QA passed the frozen
  product/proof with `P0=0`, `P1=0`, and one docs-only `P2`. The same QA task
  must verify this revised doc tip before terminal acceptance; Sol-branch
  integration remains pending.

## Formal proof closure

Sol accepted the first formal proof on the first attempt at exact implementation
commit `a37e10207455933be3930e90c55b10b669cb0ef3`, parent
`222d75cfafa9153db1eb44492bf557f11b1a9091`, branch
`codex/r1-tip-fidelity-luna`. Invocation:

```powershell
.\tools\New-TipoffVisualProof.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
```

It returned `TIPOFF VISUAL PROOF PASS`, generated UTC
`2026-08-03T21:08:24.8165649Z`, at
`build\proof\tipoff-visual-orientation-a37e10207455`. The manifest is
`proof-manifest.json`, schema `tecmo.tipoff-realtime-proof/2`, SHA-256
`AAD8EF7AF9F075E5EA1F64B91C6F363A9E2959444D893F6D0C4A760368D11438`; summary
SHA-256 is
`1AA8C97B10E31368E28BAEE8232B3B892F2BC6D249F14295040ECD87D74FA78D`.
The clean-worktree requirement passed.

Formal identity: Rev1 ROM SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`;
executable `1,981,952` bytes, SHA-256
`0CA0CDAE6C837ACB2FC0C601D25830EEA1EA6FF74EEC7D0079CAFDEB99602257`,
freshness passed, warning-clean formal build log; pack `1,406,713` bytes,
`86` entries, SHA-256
`A16D873CCBBDEBEFB19F101D34569F6F1CE280943A47221956D3B036BA89FEC4`.
TPTI-2 is `7,680` bytes/FNV32 `28910BC1`/FNV64 `7EA1596E8DFAC0C1`,
payload SHA-256
`C453848A33D6B29046D48ACDB44973D9A93234457C13F4150154F35DEA8F27FB`.
Proof-script SHA-256 is
`A92F811E6EADFAEE3876E03FC2EF0725A728437CD8572CBE75F56F27240B2D93`;
checkpoint-source SHA-256 is
`C4586F9E36AAE773B47F74340115F85416BEEBDD7267FEDFAA927808D9CC4FDE`.

Inventory pass 1 contained exactly `65` PNGs named `0661..0725`; pass 2 had
the same `65` plus one facing checkpoint. There were zero name/hash/dimension
mismatches, every PNG was `640x480`, and `138` logs were nonempty with minimum
size `196` bytes. No warning/fatal/exception/failed/failure/error-colon match
occurred; `proof-incomplete.marker` was absent and the worktree stayed clean.
The `3200x1592` contact sheet SHA-256 is
`4D29B5323D21B0C0CEACE359AFE6AB55E5EE1A7B54C783629769426D31B5EB95`; left
edge `448x5108` is
`4785DD027E8180A145517C824BC4AABEEA064EA39273450E316B3CC39BDB051A`; right
edge `448x5108` is
`7E8FF07AB0CF4D1FC3EDDCF582A8F2F82F359EAE03B5729593ADAF62E3B5BBB0`.
All `65` frames were represented with black outside-active-view host margins.
Facing `640x480` checkpoint SHA-256 is
`DDE21802E85DD14AC85F8792CBB9694C0833E5DC103A1C567891B1501F6FA783`,
deterministic twice.

The MP4 is presentation-only, never acceptance proof: `98,535` bytes, SHA-256
`4215BBF4733E71D2FFE8EC2D6C16DDF60AF187B7FE1585141320B34BEB8D4C20`,
`640x480`, `65` decoded/stored frames,
`avg_frame_rate=39375000/655171`, `r_frame_rate=39375000/655171`, duration
`1.081552s` versus expected
`1.0815521269841271s`; ffmpeg SHA-256
`D1E2A156261ECC675081943197A85F08F2868784A0AF499171EDE89353EDAD31` and
ffprobe SHA-256
`70872C3FFBC43D0B2C570F9837F54D6E9A832F4CA25463E9735B6A3EC0621478`, both
8.1 full builds.

Runtime evidence observed physical-X fast pulse only at logical `662`, Away
human sample raw frame/error `0/0`, Home automatic sample at logical `683`
raw frame/error `21/11`, all `60` contest frames anchor-ordered/inward with
camera X `256`, contest-frame cap `30`, centered ball through logical `691`,
then one-pixel-per-update left travel to the `8px` cap, and valid LIVE at
logical `721` with Away possession/direction-left and holder `0`, continuing
through `725`.

Sol inspected frames `661`, `662`, `683`, `687`, `696`, `720`, `721`, `725`,
the contact and edge sheets, and facing checkpoint. Court/HUD/players/jumpers,
ball, landing, and LIVE handoff were coherent without clipping, corruption,
snap, or host-margin leakage. Private original-reference sheets were
`4E4E4D9E9E33CE245E416B84F89B0F18FEA394C2741413AF1496CCE67E6717A5`
(automatic),
`01ECAE0D235E1433263CEBC4F828A31700A690C2AB724DF8BE03B6E195125894`
(simultaneous), and
`630BC1CA5A0E33AD28257CAB22B5D8B0C5DE3213BC4E2EB90763DEF390E6C29C`
(human-vs-CPU one-down). Native preserves the accepted `640x480` center-court
arc and frame-721 handoff; visual trajectory/camera composition is approximate,
and TTDT/`$7C48` trajectory plus original tie settlement and selector-to-team/
receiver mapping remain incomplete. Frame 721 is not ROM-exact.

## Independent terminal QA lineage and closure

Independent QA task `019fc89b-05fb-7193-aef0-e483f9306279` was exactly
retitled `Tecmo R1 TIP Fidelity — Independent Terminal QA — Luna Max`, using
`gpt-5.6-luna thinking=max`, projectless; branch/worktree/base/last-good/
writable fields remained null. Repin ran from
`2026-08-03T21:19:44.948Z` through `2026-08-03T21:19:45.419Z`; QA turn
`019fc980-4691-7de1-8352-ae94a4c27508` started at
`2026-08-03T21:20:54Z`. QA-start was not separately captured; final audit was
`2026-08-03T21:34:50.1732492Z`.

At `b678beffeacd745fe438e78d323357dc6f86af95`, QA verdict was
`REVISE docs-only`, `P0=0`, `P1=0`, with one grouped current-status/closure
`P2`. Frozen implementation, static evidence, builds, focused/broad/Win32
tests, formal proof, media, and ownership all passed. This docs-only revision
addresses that P2; the same QA task must verify the revised doc tip before
terminal acceptance, and Sol-branch integration remains pending.
QA reran the warning-clean build, focused TPTI-2 harness, broad scene suite,
Win32 smoke, and formal proof; all exited `0`.

QA proof root was `build\proof\qa-tipoff-b678beffeacd`, generated UTC
`2026-08-03T21:26:21.3756164Z`, schema `tecmo.tipoff-realtime-proof/2`,
manifest commit `b678beffeacd745fe438e78d323357dc6f86af95`, manifest SHA-256
`051002DF73166C914DB236BAB1313800917C849D47026EDF1AAD30C70F4D6DEC`, and
summary SHA-256
`BE54A3110C61CCCA11502535D446C87D23A43C0105B72C22F724B8AB4C47CFBF`.
The QA executable was `1,981,952` bytes with SHA-256
`EF06845CE7622ED310BE4CDA9DAB84437662F2F60A333E0F8D2372B5A8001CFE`.
The pack was `1,406,713` bytes/`86` entries with SHA-256
`A16D873CCBBDEBEFB19F101D34569F6F1CE280943A47221956D3B036BA89FEC4`;
TPTI-2 was `7,680` bytes/FNV32 `28910BC1`/FNV64 `7EA1596E8DFAC0C1`.

QA found `65` contiguous `0661..0725` frames, deterministic pass 2, all
`640x480`, `138` nonempty clean logs, and no incomplete marker. Artifact hashes
matched the accepted a37e proof byte-for-byte despite a different executable
hash; no cause is inferred. Contact/edge/facing/MP4 hashes were respectively
`4D29B5323D21B0C0CEACE359AFE6AB55E5EE1A7B54C783629769426D31B5EB95`,
`4785DD027E8180A145517C824BC4AABEEA064EA39273450E316B3CC39BDB051A`,
`7E8FF07AB0CF4D1FC3EDDCF582A8F2F82F359EAE03B5729593ADAF62E3B5BBB0`,
`DDE21802E85DD14AC85F8792CBB9694C0833E5DC103A1C567891B1501F6FA783`, and
`4215BBF4733E71D2FFE8EC2D6C16DDF60AF187B7FE1585141320B34BEB8D4C20`.
ffprobe reported `width=640 height=480 nb_read_frames=65`,
`avg_frame_rate=39375000/655171`, `r_frame_rate=39375000/655171`, and
`duration=1.081552`. Visual review covered frames `661`, `662`, `683`, `687`,
`696`, `720`, `721`, `725`, contact/edge/facing sheets, and the three current
original sheets without corruption, clipping, or margin leakage. Native
full-court `640x480` arc remains approximate versus the original `256x224`
close-up/longer path; tie, selector/receiver, TTDT/`$7C48`, and ROM-exact
frame-721 timing remain incomplete.

### QA-only diagnostics

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 | `CreateProcess ... The directory name is invalid. (os error 267)` | QA corrected the process path and retried. | No mutation. |
| 1 | PowerShell `||` `ParserError` | QA replaced the unsupported separator with PowerShell-compatible control flow. | No mutation; bad-request count `0`. |
| 1 | Malformed `rg` regex with an unrecognized escape | QA corrected the regex escaping. | No mutation. |
| 65 | Accepted-frame comparison falsely reported `MISSING` because it read a nonexistent pass property. | QA switched to frame-plus-SHA fields; all `65x2` comparisons matched. | No mutation. |
| 1 | Image inventory `InvalidCastFromStringToInteger` on `away-left-facing` | QA corrected the inventory regex. | No mutation. |

### Sol read-only post-QA tree diagnostic

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 3 | `fatal: ambiguous argument 'dAByAGUAZQA=': unknown revision or path not in the working tree. Use '--' to separate paths from revisions...` | Unquoted `^{tree}` was interpreted by PowerShell; quoted revisions recovered the exact tree hashes. | No mutation; literal bad-request count `0`. |

### Commit-audit diagnostic

| Count | Raw signature / purpose | Cause and recovery | State impact |
|---:|---|---|---|
| 1 command, 3 reported lines | Initial `git diff --cached --check` reported `docs/finish-tasks/R1-tip-fidelity/README.md:8: trailing whitespace.`, with the same diagnostic at lines 10 and 11. | Three pre-existing Markdown hard-break spaces were removed from the authorized README; the cached diff check was rerun and passed. | No implementation, repository-state, or external-state impact beyond the authorized documentation cleanup. |
| 1 | `At line:2 char:200 ... The token '||' is not a valid statement separator in this version.` / `FullyQualifiedErrorId ParserError` | A read-only documentation audit used the Bash `||` separator under Windows PowerShell. Recovery used explicit PowerShell-compatible control flow. | No repository or external-state mutation; literal bad-request count remains `0`. |
