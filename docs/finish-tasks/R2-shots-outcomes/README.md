# Tecmo R2 Shots Outcomes

Status: implementation/revision candidate, uncommitted, pending independent QA
and final acceptance. This document describes the bounded native implementation in
the Luna worker worktree; it is not an acceptance certificate.

## Lineage and handoff

- Source orchestrator: `019fc89a-de0d-7b61-9364-3cf96ff8dba8`.
- Worker task supplied by Sol: `019fc8cb-3d2c-7171-bbe8-534028387b6e`,
  “Tecmo R2 Shots Outcomes Implementation and Revisions - Luna Max”,
  `gpt-5.6-luna/max`, created `2026-08-03T18:03:10Z`; pinned and still
  active at documentation time. The app/project/Git task fields were null;
  the manual worker branch/worktree below is authoritative.
- Evidence task supplied by Sol:
  `019fc8a8-186e-7be2-aab3-0aae3da3a2fa`, “Tecmo R2 Shots Outcomes Evidence
  Native Gap Audit - Luna Max”, `gpt-5.6-luna/max`, created
  `2026-08-03T17:24:47Z`; it was pinned and later unpinned after acceptance.
  Sol supplied that it had no bad-request/replacement fault and one ordinary
  non-material failed read command.
- Worker branch: `codex/r2-shots-outcomes-luna`.
- Worker worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r2-shots-outcomes-luna`.
- Exact base, current parent, and merge-base:
  `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- No commit has been created on this worker branch. The intended handoff is
  a Sol-reviewed fast-forward-only sequence from the exact base; no merge,
  rebase, push, or force operation has been performed.

The revision lineage contains the Sol-supplied live-review rounds covering
transactional loaders and shot wrappers, neutral numeric-1 identity/exposure, direction
signs, source family/profile/direction selectors, evaluator typing and
contact/contest classification, exact/approximate schedules, rattle/tail
routes, claimant source order, deep state validation, production matrices,
terminal score/possession behavior, and cleanup of temporary diagnostics.
The final review corrections in this lane include the resolver-valid future
holder fixture, jump-tail base/duration binding, normal diagnostic-selector
zero binding, fixed make gather-entry binding, exact both-team score checks,
removal of the temporary production failure-reason API, and the R1 correction
that raw A7A9 metadata on a MAKE never activates the miss-only rattle bit. The
R1 exact and native-approximate A7A9 MAKE regressions both pass: the exact
bounded search resolves team 0/roster 0, vector 0 (`dx=-320,dy=64`), launch
frame 42, sample `AC0D3E09`, probability 5. Sol visual review is complete;
independent QA and final acceptance remain pending.

## Scope

This lane owns the shot/outcome boundary in native C:

- transactional TGCS/TGJS/TGDK/TGSR parse/load and precise fresh-destination
  diagnostics;
- close numeric identities 0/1/2, including the source-backed numeric-1
  fixed pose group and all eight direction slots;
- TGJS family/profile/direction selection and persisted flight-pose use;
- structured deterministic make/miss evaluation with bounded shot-local
  contact/contest metadata;
- exact captured three-point schedules where the source contract is proven;
- native approximate ordinary two-point arcs and unproven flight substitutions;
- A708 selector 0/3, A7A9 rattle, and A8E9 tails with raw identity retained;
- eligibility-ordered claimant handoff and same-team/other-team possession
  settlement without rebound/block/steal labels;
- transactional period-expiry, scoring, audio, active-shot validation, and
  deterministic replay tests in the owned scene contract.

Non-goals are defense completion, fouls, restarts, rules/statistics,
rebound/block/steal semantics, full CPU autonomous jump/far launch behavior,
unproven `$91BC` helper inputs, full `$AD6E` flight inputs, shared R1 TIP
integration, source-map metadata correction, and presentation completion.

## Evidence matrix

All ROM/ASM/decompilation/capture references below are research/test evidence
only. They are not runtime dependencies and no proprietary evidence is
committed. The canonical research ROM fingerprint for every row is:

`Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`, 393232 bytes,
SHA-256 `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

| Contract | Source-pinned evidence and pack | Implementation classification | What it proves / limits |
| --- | --- | --- | --- |
| TGCS-1 close numeric 0/2 | Bank05 `$8BDE->$8C79->$8C7D`; fixed pose/source spans; TGCS-1, 3144 bytes, FNV32 `DACDC976` | Exact/source-pinned pose/phase metadata; native faithful selector integration | Numeric 0/2 complete source semantics remain available. It does not prove numeric-1 full object trajectory. |
| TGCS numeric 1 | Bank05 `$8BDE->$8C79->$8C7D`, `$8C7D-$8CD4`; fixed group `$10`, `$8CED-$8D3C`; offsets `05E6,05A6,05C6,05D6,05B6,0586,05F6,0596`, pointer indexes `755,723,739,747,731,707,763,715` | Native approximation, source-backed pose-only exposure | `$99==1` selects the fixed group and adds the exact eight-direction slot. Profile is ignored for this group. Complete object animation/trajectory semantics remain incomplete; no dunk/layup/contact label is assigned. |
| TGSR profile bit | Bank02 `$A89E-$A8C9`: `$A8AE` loads profile byte 2, `$A8BA` applies `AND #$20`, `$A8BC` stores actor `$04B0`; Bank05 `$842C/$8C7D` consume the bit | Exact/source-pinned profile selector; native faithful integration | Existing `TecmoTeamDataPlayer.profile[2]` bit 5 selects both close/jump profile paths. It proves the bit gate, not unavailable raw roster state beyond that byte. |
| TGJS jump matrix | Bank05 `$842C-$845E` consumes family base, `$04B0 & $20` profile, and direction; TGJS-2, 2776 bytes, FNV32 `A66EE873` | Exact/source-pinned selector matrix; native faithful playback consumption | Both families × both profiles × eight directions are selected, persisted, replayed, and used during flight. Unavailable raw NES state is replaced by documented geometry/stable-sample gates. |
| TGDK | TGDK-1, 20272 bytes, FNV32 `E02F2D21`; captured dunk schedule/cue contract | Native faithful preserved schedule with transactional boundary | Existing dunk cutaway behavior and cue boundaries are retained. Full presentation mapping is not claimed. |
| TGSR point/scoring classification | Bank05 `$B995-$BA3F` | Exact/source-pinned point-class pieces; native faithful geometry binding | Point class is recomputed from immutable launch geometry and captured hoop endpoint; unavailable full helper inputs remain outside the exactness claim. |
| TGSR terminal polarity | Bank05 `$91BC-$943A`, including `$933B/$942D` make and `$9434` miss branches | Exact/source-pinned terminal polarity at supported contexts | Captured terminal polarity and supported exact schedule are retained; full `$91BC` inputs are not proven. |
| TGSR route dispatch | Bank02 `$A6EE`: low2 `0->$A708`, `1->$A7A9`, `2->$A8E9`, `3->$A708` | Exact/source-pinned raw route identity; native approximate tail motion | All four raw selectors retain selector/kind/address. A708 0/3, A8E9, and A7A9 are observably distinct in production. |
| TGSR rattle | `$A7A9/$A854/$A2DF/$AD4E/$BDF3`; TGSR-3, 512 bytes, FNV32 `164DC568`, FNV64 `5C5170460C8305A8` | Exact/source-pinned four-pass rattle contract | Altitude `$38`, repeat cadence, orientation snap, object state, and terminal bridge are bound. Full `$AD6E` inputs and rebound meaning remain unproven. |
| TGSR claimant eligibility/order | Bank05 `$B73E/$B80E` | Native approximation with source-order substitution | Eligibility and proven team-relation inputs are used; active actors are scanned in bounded source-slot order. Complete dynamic scan order is unproven and no rebound/steal label is used. |
| TGSR settlement/scoring | Bank05 `$B87C-$B8F5` settlement; `$B995-$BA3F` scoring/classification context | Native faithful terminal boundary with source-pinned polarity | Same-team handoff retains possession; opposing-team handoff changes it; makes award 1/2/3 points transactionally. Exact claimant dynamic ordering and full score-helper inputs remain unproven. |

Confidence is high for the listed byte/table identities and exact captured
polarity, medium for native selector substitutions and bounded arcs, and
incomplete for the explicitly unproven helper inputs and semantics.

## Changed implementation seams

Owned changes are limited to the files listed by the task. The principal
functions are:

- `tecmo_gameplay_close_shots_parse/load`,
  `tecmo_gameplay_jump_shots_parse/load`,
  `tecmo_gameplay_dunk_cutaway_parse/load`, and
  `tecmo_gameplay_shot_resolution_parse/load`: independent candidate parse,
  commit-on-success storage ownership, bytewise valid-to-invalid preservation,
  and precise fresh diagnostics;
- close numeric selection and pose helpers, including the neutral
  `source_variant1_gate` and fixed group `$10` resolver;
- `scene_start_shot_actor_mutating`, `scene_update_shot`, rim-tail/rattle
  startup and update paths, `scene_finish_shot`,
  `scene_finish_jump_miss`, and approximate/exact make updaters;
- `scene_select_shot_claimant` and settlement relation handling;
- `scene_shot_bound_evaluation_valid`, selector/sample/context-signature
  binding, `scene_shot_pose_state_valid`, exact/approx timeline replay,
  ball-position Q8 replay, route/tail/rattle validation, and inactive clear
  invariants;
- owned CLI/state-flow tests for exhaustive matrices, malformed input,
  overflow, replay, terminal settlement, claimant order, and rollback.

`shot_context_signature` is a redundant binding of the captured stable sample
and launch-time contact/contest classification. It is not an independent
post-launch proximity recomputation; claimant movement after launch remains
allowed and is validated separately.

The public `tecmo_gameplay_scene_draw` comment now states that selected close
and jump profile/direction entries are consumed, that only the captured
three-point schedule is exact, and that other arcs/full unproven inputs are
native approximations; it does not claim presentation completion.

## Required exclusions and known follow-ups

`src/tecmo_gameplay_scene.c` remained completely untouched because it is held
by the active R1 TIP boundary. Production numeric-1 mechanics are exposed
neutrally through the owned shot contract, but the public
`tecmo_gameplay_scene_shot_name(NUMERIC_1)` helper still returns `"invalid"`.
The accepted future one-line `"numeric-1"` case is a sequential,
non-product-mechanics integration follow-up after R1 releases `scene.c`; name
integration is incomplete.

`src/asset_pack/tecmo_asset_pack_source_map.c` also remained completely
untouched. Both excluded shared files are confirmed untouched in this lane.
TGCS-1 source-map metadata remains exact/complete for numeric 0/2,
while numeric-1 full trajectory semantics are incomplete; the pose-only
numeric-1 runtime exposure is a native approximation. The stale
“intentionally unexposed” wording is recorded as a known incomplete metadata
boundary and deferred sequential correction. No source-map test was weakened.

Other excluded domains remain untouched, including R1 TIP/pretip/render,
scene actors/live implementation, ball/dribble production code, defense,
fouls/rules/restarts, clocks/lineups/fatigue, audio assets, game flow, Win32,
build/CMake, and global source-map/import layout outside the owned files.

## Automated results

The following commands were run in the worker worktree with
`TECMO_SKIP_SHORTCUT=1` for the build path. Build output was scanned for
warning/error text and was clean.

```powershell
$env:TECMO_SKIP_SHORTCUT='1'; & .\build.ps1
$env:TECMO_SKIP_SHORTCUT='1'; & .\build\tecmo_port.exe --gameplay-scene-test build\r2-sol-review.assetpack
& .\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes' -ProofRootPath 'build\r2-scene-terminal-final'
```

Observed results:

- warning scan: `BUILD_WARNING_SCAN_CLEAN`, exit 0;
- direct scene self-test: `GAMEPLAY SCENE SELF TEST PASS`, exit 0;
- TGCS focused runner: canonical/provenance/reload, 208 poses, malformed and
  missing/cross-pack dependency cases, and 43 Rev1 endpoint/interior
  mutations, exit 0;
- TGSR focused runner: source spans, point classification, terminal polarity,
  A708/A7A9/A8E9 routes, four-pass rattle, and claimant settlement, exit 0;
- full scene runner: `GAMEPLAY SCENE TEST PASS`, including TGDK/TGJS/TGSR
  matrices, shot-clock/expiry, transactional malformed cases, rendering
  checkpoints, and proof manifest generation, exit 0.

The accepted CPU/LIVE behavior remains covered by the existing scene actor
state-flow fixtures: CPU close playback is supported, while both teams’ CPU
jump/far requests remain deferred/non-launch. No autonomous CPU jump/far
completion is claimed.

### Automated personal-run regression after Sol's proof

The supplied parent proof root was already populated by Sol's personal proof,
and the scene wrapper intentionally rejects a nonempty proof root. To preserve
those artifacts, the successful wrapper rerun used the fresh child root
`build/r2-sol-personal-scene-20260803T231543Z/automated-scene-run-r1`; the
direct self-test used the supplied parent asset-pack copy:

```powershell
$env:TECMO_SKIP_SHORTCUT='1'
$build_output = & .\build.ps1 2>&1
& .\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes' -ProofRootPath 'build\r2-sol-personal-scene-20260803T231543Z\automated-scene-run-r1'
& .\build\tecmo_port.exe --gameplay-scene-test 'build\r2-sol-personal-scene-20260803T231543Z\asset-pack\gameplay-proof.assetpack'
```

Observed results: warning-clean build, TGCS pass, TGSR pass, full scene proof
pass at the fresh child root, and direct `GAMEPLAY SCENE SELF TEST PASS`, all
exit 0. The parent proof root and supplied personal artifacts were not deleted
or overwritten.

### Post-R1 terminal-candidate provenance

Sol's post-R1 `/W4` build with `TECMO_SKIP_SHORTCUT=1` exited 0. Its diagnostic
scan count was 0 and it reported `BUILD_WARNING_SCAN_CLEAN`. Sol personally
reverified the canonical Rev 1 ROM at 393232 bytes with SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
The TGCS focused runner and TGSR focused runner both passed. A fresh full
scene proof root, `build/r2-sol-terminal-20260803T183632Z`, produced
`GAMEPLAY SCENE TEST PASS` and its manifest; the direct self-test against
`build/r2-sol-terminal-20260803T183632Z/asset-pack/gameplay-proof.assetpack`
produced `GAMEPLAY SCENE SELF TEST PASS`. These results are terminal-candidate
provenance, not independent QA or final acceptance.

## Sol-owned ignored proof provenance and visual-proof boundary

The following local artifacts and observations were supplied by Sol and are
recorded without copying or committing the artifacts:

- make session: `build/r2-original-proof/20260803-155020`, pilot pass,
  `$91BC->$933B->$942D`, `$B995`, `$BA02`, score `0-0 -> 3-0`, FCEUX SHA-256
  `F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`, FM2
  SHA-256 `AD49E96E5D87393215FAF75468ECBFE708B9D92793A9227708A9B699C4516C12`,
  proof video SHA-256
  `A243BC5AEAB46B2AFF59FFDFA2EC6460CDA01E0A4175784C2F6CFA34AC312ED2`,
  contact sheet SHA-256
  `835E74DAA79F567240DD0B12B8B6C7451A6E67495CA5214FD0D1D9BB46B3346B`;
- miss session: `build/r2-original-proof/20260803-155720`, pilot pass,
  `$91BC->$933B->$9434`, no `$BA02`, `$B87C` settlement, score `0-0`, proof
  video SHA-256
  `71FB6699D1E93587588C5B0C976182184FA0655A93067919A4DAD9AB95F96BBE`,
  contact sheet SHA-256
  `4D2412F1D72164F659FC9C6A384F9EC0EDCF6B03E2DB7E19FE06D34CB47AE8D0`;
- read-only local harness inputs: `temp-videos/gameplay-shooting-lab/shooting_lab.lua`,
  SHA-256 `E55AA58085C272B5CE88ED5201E84BA974F874C8930623CDAEFFF735FADACA9D`,
  and `Start-ShootingLab.ps1`, SHA-256
  `DFCC3A592027609BEA0C441D1CC22E3FD5FDBF46C59865871A806E8100FC4E9C`.
  Sol reports make used `RECORD_MOVIE=1/DETAILED=1` and miss used
  `RECORD_MOVIE=0/DETAILED=1`, both `MODE=shoot-once/MAX_FRAMES=5000`.

These are research-only ignored inputs and outputs, not runtime dependencies.
This section records Sol-supplied numbered full-resolution
make/miss/rattle/dunk frames, contact sheets, and videos after Sol's completed
deterministic visual inspection; no Luna visual acceptance is implied. The
personal visual observations (continuous make release/arc/rim arrival and
final BULLS 003; continuous miss arc/tail/claimant with HUD 000) are Sol-owned
observations supplied for provenance. Luna did not independently inspect or
certify those visuals.

### Sol personal native proof (ignored, personally generated and inspected by Sol)

Sol supplied and personally inspected a second, independent two-pass proof at
`build/r2-native-proof-20260803T231642Z`, using the ignored asset pack
`build/r2-sol-personal-scene-20260803T231543Z/asset-pack/gameplay-proof.assetpack`.
Each pass contains numbered `frame-0001.png`-style full-resolution frames at
640x480. The four scenarios contain 51 selected frames per pass, 102 total;
bad dimensions: 0; all 51 corresponding frame hashes match between passes.

The source checkpoint order and pass-equal aggregates are:

| Scenario | Source checkpoint order | Aggregate SHA-256 | Video SHA-256 | 320x240 tiled sheet SHA-256 / dimensions |
| --- | --- | --- | --- | --- |
| jump-make | 1,5,9,19,20,39,57,63,85,110,111 | `47D5B332BCFEFEA472C5CA4FDFCDC9A646FC5CE9E3FD208C6686807B2F74BB99` | `CD547484254BA1C67A420102A4B09114A135B3FA9857EA8506A3654766E9A8C5` | `412E121847106ED410D59C936BB1982BD9AC5345371653E1BDDE43D89479E941`, 1300x736 |
| jump-miss | 1,2,4,21,22,39,40,45,46,72,73,74,75,86,87 | `B1E87ECF18121FE57348C46326C1C54A5657FB6633362A36AEAB852E030B9AEF` | `02FAC11DF68BCBB3B3728ABA50619BCAEB73086E7880F9A5116DE87E05DD4553` | `0632810FC0AC463BD0E04C7BDF048F25D3A9C676ABD3EC9AB67DDDEA27DF6ACD`, 1300x980 |
| jump-rattle | 1,4,21,40,72,73,74,77,81,85,88,89,90,91,102,103 | `9447A2693D285FE702B3C7D67CF7D554C4962CB3B7122F0845FE633C8230AF5A` | `3606F85489E2536101703DCA3091CCF69D932BD9A78AE517050C307CEE41EFAB` | `16F904FEB371AFB904FEB842002BFC889740B486E8DB376BB5F70B8BE8D486A5`, 1300x980 |
| dunk | 1,16,32,48,64,75,80,87,132 | `F2C128049E5E28BC9750F6A55011FE2B7065D97D9CFE18B329C43DCEA588CEFD` | `CABFF4F191B4A3DFC2EB592273A94F007DDF66C19003A9AAC8B280E539EB82A0` | `69C13F83DF5E94D576A5AC6A567A3328925AA292091EA9460406E308E9EA0FAA`, 1300x736 |

All video and tiled-sheet SHA-256 values are byte-identical across the two
passes. The terminal CLI states were: make at frame 111, `shot=none`, score
`3/0`, clock `2:59`; miss at frame 87, `shot=none`, score `0/2`, clock
`2:59`; rattle at frame 103, `shot=none`, score `0/2`, clock `2:58`; and dunk
at frame 132, still `shot=dunk` at the last supported visible/resolve
checkpoint. Dunk settlement is covered by automated terminal tests and is not
claimed from this visual mode.

Sol's visual observations were: make gather/release, leftward arc/apex/rim
arrival, landing/recovery, HUD `000->003`, and clean live return; miss
release/arc/rim-tail, unchanged score, and clean `shot=none` return; rattle
flight followed by distinct multi-frame rim motion and clean score-unchanged
return; dunk court approach, source-backed cutaway staging, an intentional
full-black frame at source frame 64 matching
`TECMO_GAMEPLAY_DUNK_CLEAR_FRAME`, then court rebuild/route resume through
frame 132. HUD/court/camera were stable with no visible corrupt sprites,
clipping, or torn transition. The rattle visual mode is the deterministic
diagnostic wrapper; normal production A7A9 selection is separately proven by
the bound automated route/terminal matrices.

After the post-R1 fix, the native rerender root
`build/r2-native-proof-terminal-20260803T184000Z` used the fresh terminal
asset pack above. It produced two independent passes with 51 selected,
numbered frames per pass (102 total), all 640x480, zero bad dimensions, zero
pair mismatches, and zero direct per-frame mismatches against the previously
inspected `build/r2-native-proof-20260803T231642Z/repeat-1` baseline. Sol
personally reinspected the four existing contact sheets after the underlying
baseline frames were proven byte-identical to the post-fix rerender. The
observations remain coherent: make release/arc and `003` settlement; miss
arc/tail with unchanged score; distinct rattle motion and clean handoff; and
dunk approach, cutaway, source frame 64 full-black, and court return, with no
visible corrupt sprites, clipping, or torn transition.

The PowerShell reproduction outline is:

```powershell
$ProjectRoot = (Resolve-Path '.').Path
$Exe = Join-Path $ProjectRoot 'build\tecmo_port.exe'
$AssetPack = Join-Path $ProjectRoot 'build\r2-sol-terminal-20260803T183632Z\asset-pack\gameplay-proof.assetpack'
$OutputRoot = Join-Path $ProjectRoot 'build\r2-native-proof-terminal-20260803T184000Z'
$BaselineRoot = Join-Path $ProjectRoot 'build\r2-native-proof-20260803T231642Z\repeat-1'
if (Test-Path -LiteralPath $OutputRoot) {
  throw "Refusing to overwrite existing proof root: $OutputRoot"
}
if (-not (Test-Path -LiteralPath $BaselineRoot)) {
  throw "Missing byte-comparison baseline: $BaselineRoot"
}
New-Item -ItemType Directory -Path $OutputRoot | Out-Null
$env:TECMO_ASSETPACK = $AssetPack
$Cases = [ordered]@{
  'jump-make'   = @{ prefix='gameplay-jump-make-frame'; frames=@(1,5,9,19,20,39,57,63,85,110,111) }
  'jump-miss'   = @{ prefix='gameplay-jump-frame';      frames=@(1,2,4,21,22,39,40,45,46,72,73,74,75,86,87) }
  'jump-rattle' = @{ prefix='gameplay-jump-rattle-frame'; frames=@(1,4,21,40,72,73,74,77,81,85,88,89,90,91,102,103) }
  'dunk'        = @{ prefix='gameplay-dunk-frame';     frames=@(1,16,32,48,64,75,80,87,132) }
}
foreach ($pass in 'pass1','pass2') {
  foreach ($name in $Cases.Keys) {
    $dir = Join-Path $OutputRoot (Join-Path $pass $name)
    New-Item -ItemType Directory -Path $dir | Out-Null
    $n = 1
    foreach ($sourceFrame in $Cases[$name].frames) {
      $mode = "{0}{1}" -f $Cases[$name].prefix,$sourceFrame
      $png = Join-Path $dir ("frame-{0:D4}.png" -f $n)
      & $Exe --render-test-mode $mode $png
      if ($LASTEXITCODE -ne 0) { throw "Render failed: $mode" }
      $baselinePng = Join-Path $BaselineRoot (Join-Path $name ("frame-{0:D4}.png" -f $n))
      if ((Get-FileHash -LiteralPath $png -Algorithm SHA256).Hash -ne
          (Get-FileHash -LiteralPath $baselinePng -Algorithm SHA256).Hash) {
        throw "Baseline mismatch: $name frame $n"
      }
      $n++
    }
    & ffmpeg -y -framerate 6 -start_number 1 -i (Join-Path $dir 'frame-%04d.png') `
      -frames:v $Cases[$name].frames.Count -an -pix_fmt yuv420p `
      (Join-Path $dir "$name.mp4")
    # Resize each PNG to 320x240 in PowerShell/System.Drawing and place them
    # in four columns and ceil(count/4) rows for the tiled contact sheet.
  }
}
```

For each case, the aggregate is SHA-256 of the UTF-8 bytes of the ordered
uppercase per-frame SHA-256 strings joined by CRLF (`0D 0A`), with no trailing
separator: `SHA256(($FrameHashes | % { $_.ToUpperInvariant() }) -join "`r`n")`.
The post-fix terminal rerender reproduces all four documented aggregate hashes
in both passes under this CRLF formula. The MP4 and tiled-sheet files are then
hashed with `Get-FileHash -Algorithm SHA256`. The root, asset pack, frames,
videos, and sheets are ignored research artifacts and are not runtime
dependencies or committed evidence.

## Approximations and incompletes

1. Full `$91BC` helper inputs and `$AD6E` flight inputs are unavailable; native
   evaluation and arcs are explicitly labeled approximations except for the
   captured exact pieces listed above.
2. Numeric-1 uses the validated fixed pose group and a bounded 24-frame held
   pose schedule. Full object/trajectory semantics and semantic naming are
   unknown.
3. The family gate uses captured geometry plus stable-sample bit 13 because
   raw `$038A/$006A` state is unavailable. The close-vs-jump vertical
   substitution uses stable-sample bit 9. These are neutral substitutions,
   not contact/foul/family semantic claims.
4. Claimant traversal is active actor-slot/source order. Native dynamic scan
   order remains unproven. Same-team retention and other-team possession
   change are classified only as settlement relations.
5. One-point production controller selection is not claimed: the normal
   controller launch uses `shot_flags=0` and selects 2/3; one-point coverage
   is a coherent settlement-only fixture.
6. Presentation beyond the existing TGDK/camera/HUD contracts is incomplete.

## Fault ledger and exact final state

See [FAULT-LEDGER.md](FAULT-LEDGER.md) for the ordinary failed commands,
diagnostic interpretations, and fixes retained in this uncommitted lineage.
At the time of writing, `git diff --check` is clean, all changed paths are
owned paths, and explicit zero-diff checks confirm that both excluded shared
files, `src/tecmo_gameplay_scene.c` and
`src/asset_pack/tecmo_asset_pack_source_map.c`, remain untouched. The worker
is still at the exact base commit with no candidate commit created. Independent
QA remains pending, and the shared-file name/source-map follow-ups remain
incomplete and deferred.
