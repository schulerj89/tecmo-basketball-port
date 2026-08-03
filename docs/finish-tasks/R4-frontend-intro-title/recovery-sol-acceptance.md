# R4 frontend/intro/title recovery Sol acceptance

Status: replacement-Sol implementation, proof review, and recovery verification
complete at source/test tip `d12073511c80f7eef8b606776415db20b16623a6`.
This document is the authoritative current-status record for the recovered R4
frontend/intro/title candidate. The exact commit that adds this document is
reported in the signed Sol handoff because a commit cannot contain its own
content-addressed SHA.

The three Luna documents beside this file remain immutable lineage-time
evidence. Their statements such as "awaiting Sol's ordered merge," their
historical test counts/report hashes, and their worker-local merge commands
describe the state when those worker revisions were committed. They are not
the current integration status and are superseded by this recovery record.

## Recovery and fault record

- Authoritative master task: `019fc5d4-f360-78b3-b2a6-c8bae92df690`.
- Failed predecessor Sol task: `019fc822-b4cc-7122-815e-6c63e03d9235`.
- Raw predecessor task status: `systemError`.
- Preserved predecessor last-good: `a40dc3f9976d40444d91255f31a29959f5b23be3`.
- Replacement Sol task: `019fc8e8-190e-7912-937f-482a325dfa52`.
- Replacement role: master-created `gpt-5.6-sol / max` recovery orchestrator.
- Takeover was confirmed before mutation from clean worktree
  `C:\Users\joshs\Projects\tecmo-basketball-port-r4-frontend-intro-title-sol`,
  branch `codex/r4-frontend-intro-title-sol`, exact HEAD/last-good
  `a40dc3f9976d40444d91255f31a29959f5b23be3`.
- Required base and takeover merge-base:
  `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.
- `main` and `origin/main` were both master-owned at that base during takeover.
  They subsequently advanced outside this worktree. At the final-documentation
  observation `2026-08-03T14:34:39.030-05:00`, both were
  `bcacd5b6963f4db1a92c8db9b9770413505a0e98`; the candidate/main merge-base
  remained `6d8f9c7...`. Replacement Sol did not mutate either ref.
- The failed task, every worktree/branch, accepted commit, and ignored proof
  artifact were preserved. No force, reset-hard, destructive clean, rebase,
  main merge, origin push, worktree deletion, or cross-domain edit occurred.

This is the required bad-request/fault continuity record: predecessor ID,
literal raw `systemError`, preserved last-good, replacement ID, and confirmed
takeover state are all explicit. The replacement encountered no equivalent
request fault.

## Ownership and changed-path boundary

The accepted lineage stays within `OWN-R4-FRONTEND-INTRO-TITLE`:

- `include/tecmo_intro_*.h` and `src/tecmo_intro_*.c`;
- `include/tecmo_title_screen.h` and `src/tecmo_title_screen.c`;
- owned intro/title asset-pack implementation;
- `tools/Run-IntroSequenceTests.ps1`;
- `docs/finish-tasks/R4-frontend-intro-title/**`;
- the separately granted proof-only boundary
  `src/tecmo_cli_render_scene_modes.c`.

No audio, menu/data/gameplay, build/CMake, source-map/import-layout, Win32,
README, PORTING, AGENTS, master state, or other tracked path is in the range.

## Accepted commit and worker lineage

The immutable domain base is
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.

### Native contract hardening

- Luna task `019fc848-b87f-7e32-8954-51097efa933a`, title
  `Tecmo R4 Frontend Intro Title — Native Contract Hardening — Luna Max`,
  `gpt-5.6-luna / max`, branch
  `codex/r4-frontend-intro-title-native-hardening-luna`.
- Worker lineage:
  `f1e7005115f98d0ef189639a2364e740246b6d8c` ->
  `175b28e5dba1b0020cae09958a473ba496b015e2` ->
  `a0b11042e5c479d5b6a5c738e8d51b532439b073`.
- Accepted Sol merge:
  `761e061fdea1fcb07c367d406b785337e7366919`, with parents
  `6d8f9c7...` and `a0b11042...`.

### Finale/title fidelity

- Worker branch `codex/r4-frontend-intro-title-finale-luna`.
- Worker lineage:
  `6000a0504e8a640c17698411103090d105f21245` ->
  `e97f2d2498bf96ee7a552e71e9f87d1eadce8456` ->
  `3e7496608b5ab1e6c9e8de593a62d2279be34b40`.
- Accepted Sol merge:
  `a40dc3f9976d40444d91255f31a29959f5b23be3`, with parents
  `761e061...` and `3e749660...`.

### Production replay proof

- Inherited and reused proof Luna task
  `019fc845-74e2-70d3-ad7e-dcee727a66c4`, exact title
  `Tecmo R4 Frontend Intro Title — Production Proof Replay — Luna Max`,
  `gpt-5.6-luna / max`, branch
  `codex/r4-frontend-intro-title-proof-luna`, worktree
  `C:\Users\joshs\Projects\tecmo-basketball-port-r4-frontend-intro-title-proof-luna`.
- Worker source/document lineage:
  `853e46ac3151cc80fdc432b9784e40e80f0edf1c` ->
  `fee87cfd831b82a75ffa8abccaabbfe8e9022115` ->
  `776db08ee45d694390e0d8133cdcd4934bdca3d4` ->
  `542b681db7f21d47d12dd6c3b175be0d57ed4db3` ->
  `1b51c471dcc5ed6117e4b3f55621782ad89fd039`.
  `fee87cfd...` is retained but explicitly superseded proof documentation.
- Replacement Sol personally accepted stable clean tip `1b51c47...` only after
  source, document, manifest, frame, contact-sheet, and video review.
- Accepted Sol no-ff merge:
  `0a3fc57f0a382378e89b8eca747ce6d3c1644dba`, with parents
  `a40dc3f...` and `1b51c47...`.

### Recovery corrections

- `c518f900584a9007b9f91ce9f75a7521702feca3`, parent `0a3fc57...`,
  `Fix finale asset contract recovery gaps`.
- `d12073511c80f7eef8b606776415db20b16623a6`, parent `c518f90...`,
  `Tighten finale CHR rejection proof`.
- This acceptance document is the ordered docs-only successor to `d120735...`;
  its exact SHA is supplied in the signed Sol report.

## Independent QA registration and results

Exactly one new independent QA task was created, as authorized. It is not a
Git worker and has no writable repository scope.

- Thread ID: `019fc8f8-44b5-7a93-835b-1c75f69ab506`.
- Exact title:
  `Tecmo R4 Frontend Intro Title — Independent Recovery QA — Luna Max`.
- Model/thinking: `gpt-5.6-luna / max`.
- Created at: `2026-08-03T18:52:21.000Z` (`1785783141`).
- Pin state during review: pinned by replacement Sol.
- Allocation: top-level projectless/null-Git; Git branch, worktree, base, and
  last-good fields are `null`.
- Projectless directory:
  `C:\Users\joshs\Documents\Codex\2026-08-03\tecmo-r4-frontend-intro-title-independent-recovery-qa-luna`.
- Reference allocation: read-only inspection of this Sol worktree/branch at
  each exact frozen candidate SHA.
- Fault/retry lineage: first creation attempt succeeded; zero creation faults,
  zero retries, zero duplicates, zero forks, and no replacement task.
- Status at this record: idle after completed read-only audits; retained and
  reused for the final stable-commit review.

The first frozen audit of `0a3fc57...` found one release-blocking synthetic
`--assetpack-test` regression and one CHR-fingerprint contract gap. Replacement
Sol independently reproduced the self-test failure before acting. The auditor
also identified stale lineage-time documentation. The recovery commit
`c518f90...` resolved both production defects:

- the public ROM builder continues to enforce exact Rev1 team-color and
  caption-name provenance;
- only the explicit synthetic `enforce_revision_fingerprints=0` path uses
  bounded SUNS/SPURS/BULLS fixture names and the production extra-glyph
  sentinel shape;
- finale `chr/all` availability now requires exactly 262144 bytes with
  FNV1a64 `0x96A64F53B240ABB4` on first and later checks;
- the canonical runner directly executes `--assetpack-test` and mutates the
  CHR fingerprint as a negative runtime case.

The second frozen audit of `c518f90...` found no remaining production-code
defect and marked both original defects resolved. It requested this current,
authoritative status document and noted that the negative assertion could be
more exact. `d120735...` resolves that test-only point by requiring
`finale=0 chr=1 schema=TFIN-1`: the shared CHR remains structurally available
while the exact finale identity gate rejects the mutated fingerprint.

The same pinned QA task performs the post-document frozen-tip audit. Its final
result and exact audited acceptance-doc commit are recorded in the signed Sol
handoff after this document is committed; this is an ordering fact, not an
unresolved SHA placeholder.

## Source and ASM evidence

The canonical research/test image is Rev1, size `393232`, SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
It is not committed or a runtime dependency.

High-confidence hardening anchors:

- Bank04 `$88E7` wait `#$96` (`150`), `$892C` two-frame loop, and `$8983`
  completion support `150 + ((0xC4 - 1) * 2) = 540` and the accepted
  production handoff at frame 540.
- Arena screen `$18` is selected through descriptor table `$DC85`; the fixed
  decoder is `$D9F6`; sprite seed/emitter/parameters are `$8984`, `$8988`,
  and `$89BD`; fixed sprite pointers are at `$A7DB`.
- Those anchors support the semantic TATR/TATL/TASG contracts and the existing
  55-piece jumbotron plus 16-piece goal grouping.

High-confidence finale/title anchors:

- Bank04 dispatch `$82CF-$82F9` routes through `$851C`, `$83EA`, `$852E`,
  `$83AE`, and `$8310`.
- `$83AE`/`$8A6E` sources the staged team color; fixed table `$DC19-$DC35`
  gives BULLS id `$03` the exact value `$DC1C = $15`.
- Fixed Bank07 title/IRQ source is `$FE14`; Bank06 title handler `$9E50`,
  character map `$A273`, glyph quads `$AF05`, and caption names `$AC4A`
  support the semantic title/caption implementation.
- The Bank06 caption table validates SUNS id `$14`, SPURS id `$17`, and
  BULLS id `$03`; runtime does not invent a production team color/name.

The accepted capture mapping removes eight top scanlines: capture row 224
maps to native row 240. This is an explicit coordinate reconciliation, not a
claim that the native renderer emulates NES scanline IRQ hardware.

## Implemented native contracts and functions

The hardening lineage makes the TATR, TATL, TASG, arena-stage, title, and CHR
loaders transactional and fail-closed. Invalid or mismatched data is rejected
without leaving stale availability. It hardens the relevant parser/loader and
availability functions, including title CHR availability and arena tile-layer
decode/registration checks, while preserving serialized TATL-1/TASG-2 layout.

The finale lineage implements and validates:

- `validate_finale_source_contract` and
  `validate_finale_caption_name_table` in the ROM importer;
- `tecmo_asset_pack_build_finale_sequence` and TFIN-1 semantic emission;
- strict finale load/parser/CHR availability logic;
- route state, page identity, sprite OAM Y+1, brightness caps, staged slot-9
  team color, split title bands, preroll/write/tail/hold, and drawing.

The recovery correction preserves exact production enforcement while making
the central synthetic fixture honest and testable. Canonical CHR identity is
shared with the accepted arena contract: 262144 bytes, FNV1a64
`0x96A64F53B240ABB4`.

The proof source recognizes only `intro-production-clean-frame<N>`. Decimal N
is strict (digits only, no sign/trailing text/overflow) and inclusive through
4096. It enters normal `TECMO_MODE_FIRST_SPRITE`, performs exactly N neutral
`tecmo_runtime_update` calls, then uses the generic runtime renderer. It does
not directly assign intro, finale, title, attract, or scene-local counters.
Invalid names create no output.

## Final automated verification

After `d120735...`, replacement Sol ran the canonical command with the private
Rev1 path supplied only at execution time:

```powershell
.\tools\Run-IntroSequenceTests.ps1 -Build -RomPath $env:TECMO_ROM_PATH
```

Result:

- `/W4` builds passed for both `tecmo_port.exe` and `tecmo_port_game.exe`.
- `passed=true`, `test_count=29`, `skipped_count=1`, `failure_count=0`.
- `private_paths_included=false`, `raw_output_persisted=false`.
- Final report:
  `build/intro_sequence_test_report.json`, SHA-256
  `CE90E24193657C8AB10178CAFFEC1B7B18EBB996262C9007023073131690426D`.
- Generated semantic pack: 1397729 bytes, SHA-256
  `E97BC249441A11D2110D0F9E60A88CE9690C5EEBA384BA0BD023DFE17DB99886`.
- Direct `intro-assetpack-self-test`: exit 0/pass.
- `bad-chr-fingerprint`: exit 1, no PNG, runtime rejection, and exact
  `finale=0 chr=1 schema=TFIN-1` diagnostic requirement.
- Direct `--assetpack-test`: `Asset pack self-test passed.`
- Direct `--arena-scene-test`: `ARENA INTRO SCENE SELF TEST PASS`.
- Correctly configured `--root ... --flow-test` with the generated semantic
  pack: `FLOW TEST PASS: menu play-intro title start-game-menu preseason
  season quit`.
- `git diff --check` passed.

The one intentional skip is the bounded BUCKS-pass reference comparison when
its optional private reference input is unavailable. It is reported as a skip,
not a pass, and does not hide a failure in the production semantic path.

## Reproducible production proof

Ignored proof root: the detached staging worktree's `build/proof-v2`. No ROM,
pack, frame, contact sheet, manifest, video, trace, or private path is tracked.

- Detached staging HEAD: `a40dc3f...`, with only the already-committed proof
  source patch staged during generation.
- Complete production sequence: exactly 3152 numbered source frames
  `0000..3151`, each 640x480 8-bit RGBA, and 3152 contiguous state rows.
- Consolidated manifest SHA-256:
  `D1AD002A59ADB4BFC7CD83614EF26620A10D642CBB1AEF47FEA1CFBE61906FCB`.
  Its Y/U/V PSNR values are JSON strings `"inf"`, never null.
- Artifact manifest 22/22; contact-sheet entries 51/51 (50 numbered ranges
  plus one boundary sheet); selected frames 89/89; video extracts 33/33;
  strict parser outcomes 9/9; production/direct overlap 25/25; boundary rows
  33/33; representative fresh-process determinism including N=4096: exact.
- Complete production boundary: finale N=1508, attract N=2509, title reset
  N=3151. N=0 and N=3151 share black-frame SHA-256
  `2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A`.
- Accepted inclusive bound N=4096 SHA-256:
  `DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381`.
- The two deterministic videos are byte-identical, SHA-256
  `9CBB34C4AD6F401103A37E06B2ABD874D86FC7F400D946DBEEF2742F2BAB8480`.
- Fresh ffprobe: one H.264 High 4:4:4 Predictive yuv444p video stream,
  640x480, 60/1 fps, 3152 frames, 52.533333 seconds, no audio stream.
- Fresh full-sequence normalized yuv444p comparison: Y, U, V, average, min,
  and max PSNR are all infinite. This is pixel-lossless after the intended
  RGB-to-YUV conversion; it is not a claim of RGB container identity.

Reproduction shape:

```powershell
$env:TECMO_ASSETPACK = (Resolve-Path 'build\intro_sequence\tecmo_intro_sequence_test.assetpack').Path
0..3151 | ForEach-Object {
    $n = $_
    $out = 'build\proof-v2\frames\frame-{0:D4}.png' -f $n
    .\build\tecmo_port.exe --root $env:TECMO_DECOMP_ROOT `
        --render-test-mode ('intro-production-clean-frame' + $n) $out
}
```

Private environment variables must resolve to the canonical inputs. Generated
payloads remain ignored and must not be committed.

## Replacement Sol visual acceptance

Replacement Sol personally inspected source and encoded boundary sheets,
corrected range sheets spanning the complete sequence, and full-resolution
source frames 0410, 1372, 1519, 1606, 1676, 1800, 2051, 2200, 2380, 2800,
3150, plus encoded frames 2200 and 3150. The review observed:

- continuous TECMO/license presentation, arena pan/READY reveal, WARRIORS,
  CLIPPERS, BUCKS, and PASS motion;
- clean black handoffs;
- finale progression SUNS -> SPURS -> selector -> BULLS -> title
  write/hold/retract -> attract cycles -> reset;
- readable complete imagery, continuous fades/motion, and deterministic
  representative frames;
- no crop, debug overlay, stale frame, route jump, or unintended pixel loss.

This is replacement Sol's personal product/visual acceptance, not a delegated
claim. The video is intentionally silent and neutral-input replay; input flow
is proven separately by `--flow-test`.

## Honest approximations and residual limits

- Arena dynamic palette scheduling remains deferred. Static TATL palette
  rendering is accepted; an exact Bank04/control-flow plus PPU `$3F00-$3F1F`
  write trace is required before claiming a frame-indexed native schedule.
- The accepted 540-frame arena handoff retains the documented possible `$C4`
  wrap interpretation question; tests cover frames 539/540 and no change was
  made without stronger evidence.
- The legacy five-rectangle arena scene is conceptual geometry and is not
  production pixel-fidelity evidence.
- Finale capture/native mapping applies the documented eight-scanline offset.
  Three semantic title bands represent the observed split; native C does not
  emulate the original hardware scanline IRQ.
- Proof establishes the native semantic runtime path and its deterministic
  pixels. It does not claim full-ROM emulator pixel parity for every frame.
- The production video is silent, contains no audio stream, and uses neutral
  input. Audio is outside ownership; input transition proof is the flow test.
- Generated proof and the canonical ROM/decompilation remain private ignored
  evidence and are not runtime or repository dependencies.

## Master-only handoff and stop boundary

Replacement Sol stops at this accepted branch and never touches `main` or
`origin/main`.

The candidate is a descendant of domain base `6d8f9c7...`; therefore a
dedicated R4 integration/staging ref still at that base can consume the final
Sol tip with `git merge --ff-only codex/r4-frontend-intro-title-sol`. Do not
run that command in this Sol worktree, and do not use it against current main.

Current master-owned main has independent post-base commits and diverges from
this candidate at `6d8f9c7...`; direct `--ff-only` into current main is not
valid and must not be forced. Master alone owns collision audit, any deliberate
integration merge onto its current main lineage, final verification, push,
and control-plane/session registration. Do not rebase, reset, or rewrite the
accepted candidate to manufacture a fast-forward.

The signed Sol handoff supplies the final acceptance-document commit, exact
branch/head/cleanliness, post-document independent-QA result, and terminal
`ACCEPTED` signature.
