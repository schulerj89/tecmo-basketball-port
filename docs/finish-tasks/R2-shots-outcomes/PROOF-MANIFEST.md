# R2 shots/outcomes proof manifest

This manifest is reproducibility metadata only. Build outputs and original
proof recordings remain ignored and are not committed.

## Environment and source identity

- Worker branch: `codex/r2-shots-outcomes-luna`
- Worker path: `C:\Users\joshs\Projects\tecmo-basketball-port-r2-shots-outcomes-luna`
- Candidate commit: `24bdde9c87b1529d9ab83671bc8c60c1e136ceb1`, message
  `feat: complete R2 shot outcomes`
- Candidate parent/base and merge-base:
  `222d75cfafa9153db1eb44492bf557f11b1a9091`
- Candidate tree: `367c14eb390f53a7b7a45c08d9ad1a02ab44d415`; commit stat: 17
  owned paths, 7977 insertions, 420 deletions
- Candidate branch/worktree were clean after the implementation commit
- Committed QA-lineage child: `8be0258e83369bce58d3a9eabedb4ef575127b25`,
  parent `24bdde9c87b1529d9ab83671bc8c60c1e136ceb1`, tree
  `5863c301ed00e8dedbc9e2af12a3c8b97ea876f3`, message
  `docs: record R2 shot outcomes`, exactly 3 docs, 138 insertions, 30 deletions
- ROM: 393232 bytes, personally reverified by Sol post-R1, SHA-256
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`
- Build mode: PowerShell `/W4` path with `TECMO_SKIP_SHORTCUT=1`
- Earlier baseline direct-scene-run pack: ignored
  `build\r2-sol-review.assetpack`
- Post-R1 terminal-candidate direct-run pack: ignored
  `build\r2-sol-terminal-20260803T183632Z\asset-pack\gameplay-proof.assetpack`

## Reproduction commands

```powershell
Set-Location C:\Users\joshs\Projects\tecmo-basketball-port-r2-shots-outcomes-luna
$env:TECMO_SKIP_SHORTCUT='1'
& .\build.ps1
& .\build\tecmo_port.exe --gameplay-scene-test build\r2-sol-review.assetpack
& .\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes' -ProofRootPath 'build\r2-scene-terminal-final'
```

The warning/error scan returned `BUILD_WARNING_SCAN_CLEAN`. The direct scene
run and all three focused runners returned exit 0. The full scene runner
returned `GAMEPLAY SCENE TEST PASS` and generated its ignored manifest under
`build\r2-scene-terminal-final`.

## Post-R1 terminal-candidate run

The post-R1 `/W4` build with `TECMO_SKIP_SHORTCUT=1` exited 0, had diagnostic
scan count 0, and reported `BUILD_WARNING_SCAN_CLEAN`. Sol personally
reverified the canonical ROM size and SHA-256 above. The TGCS focused runner
passed and the TGSR focused runner passed.

The fresh full scene proof root was
`build/r2-sol-terminal-20260803T183632Z`. It produced
`GAMEPLAY SCENE TEST PASS` and its manifest. The direct self-test against
`build/r2-sol-terminal-20260803T183632Z/asset-pack/gameplay-proof.assetpack`
produced `GAMEPLAY SCENE SELF TEST PASS`. All commands exited 0.

The post-fix native rerender root was
`build/r2-native-proof-terminal-20260803T184000Z`, using that fresh asset
pack. It produced two independent passes with 51 selected numbered frames per
pass (102 total), all 640x480, zero bad dimensions, zero pair mismatches, and
zero direct per-frame mismatches against the previously inspected
`build/r2-native-proof-20260803T231642Z/repeat-1` baseline. The four existing
contact sheets were personally reinspected by Sol after the underlying
baseline frames were proven byte-identical to the post-fix rerender. The
actual output subdirectories are `pass1/{jump-make,jump-miss,jump-rattle,dunk}`
and `pass2/{jump-make,jump-miss,jump-rattle,dunk}`.

The reproducible post-R1 command sequence is:

```powershell
Set-Location C:\Users\joshs\Projects\tecmo-basketball-port-r2-shots-outcomes-luna
$env:TECMO_SKIP_SHORTCUT='1'
$RomPath = 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
$TerminalRoot = 'build\r2-sol-terminal-20260803T183632Z'
if (Test-Path -LiteralPath $TerminalRoot) {
    throw "Refusing to overwrite existing scene proof root: $TerminalRoot"
}
& .\build.ps1
& .\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath $RomPath
& .\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath $RomPath
& .\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath $RomPath -ProofRootPath $TerminalRoot
$PackPath = Join-Path $TerminalRoot 'asset-pack\gameplay-proof.assetpack'
& .\build\tecmo_port.exe --gameplay-scene-test $PackPath
```

Expected terminal results are `/W4` build exit 0 with
`TECMO_SKIP_SHORTCUT=1`, diagnostic scan count 0 and
`BUILD_WARNING_SCAN_CLEAN`; TGCS/TGSR focused passes;
`GAMEPLAY SCENE TEST PASS` with a fresh manifest; and direct
`GAMEPLAY SCENE SELF TEST PASS`. The render executable syntax used below is
the existing `tecmo_port.exe --render-test-mode MODE OUTPUT` interface, with
the ignored asset pack selected through `TECMO_ASSETPACK`; no unverified
`--root` argument is required for these render calls.

## Sol personal native proof

Sol personally generated and inspected two independent passes under ignored
root `build/r2-native-proof-20260803T231642Z`, using
`build/r2-sol-personal-scene-20260803T231543Z/asset-pack/gameplay-proof.assetpack`.
Each selected frame is numbered `frame-0001.png`-style and is 640x480. There
are 51 selected frames per pass, 102 full-resolution frames total, zero bad
dimensions, and all 51 corresponding repeat hashes are equal.

| Scenario | Source checkpoint order | Aggregate SHA-256 | Video SHA-256 | Sheet SHA-256 / dimensions |
| --- | --- | --- | --- | --- |
| jump-make | 1,5,9,19,20,39,57,63,85,110,111 | `47D5B332BCFEFEA472C5CA4FDFCDC9A646FC5CE9E3FD208C6686807B2F74BB99` | `CD547484254BA1C67A420102A4B09114A135B3FA9857EA8506A3654766E9A8C5` | `412E121847106ED410D59C936BB1982BD9AC5345371653E1BDDE43D89479E941`, 1300x736 |
| jump-miss | 1,2,4,21,22,39,40,45,46,72,73,74,75,86,87 | `B1E87ECF18121FE57348C46326C1C54A5657FB6633362A36AEAB852E030B9AEF` | `02FAC11DF68BCBB3B3728ABA50619BCAEB73086E7880F9A5116DE87E05DD4553` | `0632810FC0AC463BD0E04C7BDF048F25D3A9C676ABD3EC9AB67DDDEA27DF6ACD`, 1300x980 |
| jump-rattle | 1,4,21,40,72,73,74,77,81,85,88,89,90,91,102,103 | `9447A2693D285FE702B3C7D67CF7D554C4962CB3B7122F0845FE633C8230AF5A` | `3606F85489E2536101703DCA3091CCF69D932BD9A78AE517050C307CEE41EFAB` | `16F904FEB371AFB904FEB842002BFC889740B486E8DB376BB5F70B8BE8D486A5`, 1300x980 |
| dunk | 1,16,32,48,64,75,80,87,132 | `F2C128049E5E28BC9750F6A55011FE2B7065D97D9CFE18B329C43DCEA588CEFD` | `CABFF4F191B4A3DFC2EB592273A94F007DDF66C19003A9AAC8B280E539EB82A0` | `69C13F83DF5E94D576A5AC6A567A3328925AA292091EA9460406E308E9EA0FAA`, 1300x736 |

The video and sheet hashes are byte-identical between passes. Terminal CLI
state was make frame111 `shot=none score=3/0 clock=2:59`, miss frame87
`shot=none score=0/2 clock=2:59`, rattle frame103 `shot=none score=0/2
clock=2:58`, and dunk frame132 `shot=dunk`. The dunk state is only the last
supported visible/resolve checkpoint; settlement is not claimed from this
visual mode because automated terminal tests cover it.

Sol observed coherent make gather/release, leftward arc/apex/rim arrival,
landing/recovery, HUD `000->003`, and clean live return; miss release/arc/
rim-tail, unchanged score, and clean shot-none return; rattle flight followed
by distinct multi-frame rim motion and clean score-unchanged return; and dunk
court approach, source-backed cutaway staging, a full-black source frame64
matching `TECMO_GAMEPLAY_DUNK_CLEAR_FRAME`, and court rebuild/route resume
through frame132. No visible corrupt sprites, clipping, or torn transition
was observed. The rattle mode is the deterministic diagnostic wrapper; normal
production A7A9 selection is separately covered by bound automated matrices.

The observations remain unchanged after the post-fix rerender: coherent make
release/arc/`003` settlement, miss arc/tail with unchanged score, distinct
rattle motion and clean handoff, and dunk approach/cutaway/source frame64
full-black/court return, with no visible corrupt sprites, clipping, or torn
transition.

The render outline used the existing `--render-test-mode MODE PATH` interface,
mapping the four scenario names to `gameplay-jump-make-frameN`,
`gameplay-jump-frameN`, `gameplay-jump-rattle-frameN`, and
`gameplay-dunk-frameN`, respectively, with the supplied asset pack selected
through `TECMO_ASSETPACK`. Frames were encoded with ffmpeg at 6 fps using
`-framerate 6 -start_number 1 -i frame-%04d.png`; each full-resolution PNG was
resized to 320x240 and placed in a four-column PowerShell/System.Drawing tiled
sheet. Each aggregate is SHA-256 over the UTF-8 bytes of the ordered uppercase
per-frame SHA-256 values joined by CRLF (`0D 0A`), without a trailing
separator. The post-fix terminal rerender reproduces all four documented
aggregate hashes in both passes under this corrected CRLF formula. MP4 and
sheet files were hashed with SHA-256. These are ignored research artifacts.

The ordinary non-material proof-command faults were: an initial aggregate
retry used unavailable `[Convert]::ToHexString` on this PowerShell runtime,
and a later exploratory helper named `H` collided with the `Get-History`
alias. Both were read-only/ignored-output verification mistakes; `BitConverter`
and an unambiguous helper name resolved them. The first LF aggregate
recomputation mismatch exposed and corrected this prose formula to CRLF; the
actual frames never mismatched.

The docs-only personal regression sequence was:

```powershell
$env:TECMO_SKIP_SHORTCUT='1'
& .\build.ps1
& .\tools\Run-GameplayCloseShotTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplayShotResolutionTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
& .\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes' -ProofRootPath 'build\r2-sol-personal-scene-20260803T231543Z\automated-scene-run-r1'
& .\build\tecmo_port.exe --gameplay-scene-test 'build\r2-sol-personal-scene-20260803T231543Z\asset-pack\gameplay-proof.assetpack'
```

The observed personal-run results were build pass, TGCS pass, TGSR pass,
full scene proof pass, and direct `GAMEPLAY SCENE SELF TEST PASS`, all exit 0.
The requested parent proof root was preserved because the wrapper requires a
new or empty root; only the fresh child rerun root was written.

The R1 exact A7A9 MAKE search remains within its original bounded fixture
search and resolves team 0/roster 0, vector 0 (`dx=-320,dy=64`), launch frame
42, stable sample `AC0D3E09`, and probability 5. Its selector is 1, route kind
is A7A9/source `$A7A9`, and `shot_rim_rattle_selected` remains false through
the full exact settlement; the native-approximate selector-1 MAKE regression
has the same miss-only rattle guarantee.

## Sol-owned original proof sessions

These paths, hashes, and observations were supplied by Sol; no recording is
claimed as independently inspected by Luna:

| Session | Source observations | Ignored artifact hashes |
| --- | --- | --- |
| `build/r2-original-proof/20260803-155020` (make, pilot pass) | `$91BC->$933B->$942D`, `$B995`, `$BA02`, `0-0 -> 3-0` | FCEUX `F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`; FM2 `AD49E96E5D87393215FAF75468ECBFE708B9D92793A9227708A9B699C4516C12`; video `A243BC5AEAB46B2AFF59FFDFA2EC6460CDA01E0A4175784C2F6CFA34AC312ED2`; sheet `835E74DAA79F567240DD0B12B8B6C7451A6E67495CA5214FD0D1D9BB46B3346B` |
| `build/r2-original-proof/20260803-155720` (miss, pilot pass) | `$91BC->$933B->$9434`, no `$BA02`, `$B87C` settlement, `0-0` | video `71FB6699D1E93587588C5B0C976182184FA0655A93067919A4DAD9AB95F96BBE`; sheet `4D2412F1D72164F659FC9C6A384F9EC0EDCF6B03E2DB7E19FE06D34CB47AE8D0` |

The borrowed read-only local harness inputs were:

- `temp-videos/gameplay-shooting-lab/shooting_lab.lua`, SHA-256
  `E55AA58085C272B5CE88ED5201E84BA974F874C8930623CDAEFFF735FADACA9D`;
- `Start-ShootingLab.ps1`, SHA-256
  `DFCC3A592027609BEA0C441D1CC22E3FD5FDBF46C59865871A806E8100FC4E9C`.

Sol reports make environment `RECORD_MOVIE=1/DETAILED=1` and miss environment
`RECORD_MOVIE=0/DETAILED=1`, both `MODE=shoot-once/MAX_FRAMES=5000`. These
inputs are not runtime dependencies.

## Source-pinned evidence index

All entries use the canonical Rev 1 ROM SHA-256 in the environment section.

- Profile selection: Bank02 `$A89E-$A8C9`, with `$A8AE` loading profile byte
  2, `$A8BA` applying `AND #$20`, and `$A8BC` storing actor `$04B0`;
  Bank05 `$842C/$8C7D` consumes that bit. This proves the profile-bit gate;
  unavailable raw roster/runtime state remains outside the exactness claim.
- Numeric-1 close dispatch: Bank05 `$8BDE->$8C79->$8C7D`; actor state `$12`
  is `$8CE5[1]=$12` and dispatches `$9F2F`; `$0478=$0D` routes `$A006`,
  while actor/object state `$0D` at `$8CE8[1]` is a distinct object/global
  field and `$98B7` is not proof of the object dispatcher. The fixed
  numeric-1 direction offsets are pointer indexes
  `755,723,739,747,731,707,763,715`.
- Outcome/scoring: Bank05 `$91BC->$933B->$942D` make and
  `$91BC->$933B->$9434` miss polarity, `$B995-$BA3F` point classification,
  and `$B87C-$B8F5` settlement. These source pins support the captured
  polarity/classification pieces; full `$91BC` helper inputs remain unproven.
- Rim/claimant: Bank02 `$A6EE` low2 dispatches `0->$A708`, `1->$A7A9`,
  `2->$A8E9`, `3->$A708`; rattle uses `$A7A9/$A854` and related
  `$A2DF/$AD4E/$BDF3`; claimant eligibility is `$B73E/$B80E`. The native
  implementation preserves raw route identity, bounds the rattle contract,
  and uses source-slot order as an explicit claimant substitution.

`shot_context_signature` is redundant captured-launch binding for the stable
sample/contact/contest tuple, not independent post-launch proximity evidence.
The byte/table identities above are high-confidence under the shared ROM hash;
runtime substitutions and unproven helper inputs retain the medium or
incomplete classifications stated in the evidence matrix.

## Independent QA revision R2

- QA task: `019fca10-32a8-7fd0-8d8f-2f558c5d262f`, title `Tecmo R2 Shots
  Outcomes Independent QA - Luna Max`, `gpt-5.6-luna/max`, created at epoch
  `1785801487` (`2026-08-03T23:58:07Z`); pinned, top-level projectless, with
  null app-project/Git fields
- QA output directory:
  `C:\Users\joshs\Documents\Codex\2026-08-03\tecmo-r2-shots-outcomes-independent-qa\outputs`
- QA branch/worktree: `codex/r2-shots-outcomes-qa` /
  `C:\Users\joshs\Projects\tecmo-basketball-port-r2-shots-outcomes-qa`
- QA initial reference/last-good: `24bdde9c`; parent/base `222d75cf`; tree
  `367c14eb`; attempt 1; no task bad-request/replacement fault
- The persistent worker remains pinned, attempt 1, with no task bad-request or
  replacement fault. Its candidate branch/worktree were clean after commit.

QA initially considered the Three.js/browser screenshot skill. Sol corrected
the scope before any browser or product action, and QA proceeded native-only.
This was non-material and not a task fault. The initial QA verdict found one P2
stale-lineage documentation issue at README lines 3, 25-26, 386, and 391,
PROOF-MANIFEST lines 223-224, and historical FAULT-LEDGER line 4; it found no
runtime/code findings. This docs-only R2 revision resolves that P2.

Independent QA results were: exact ROM verified; `/W4` diagnostic count 0;
TGCS and TGSR focused runners passed; fresh full-scene root
`build/qa-independent-r2-scene-20260803T000000Z` passed; its direct self-test
passed; and render root `build/qa-independent-r2-render-20260803T000000Z`
contained 40 PNGs, all 640x480, with two hash-identical passes and no
diagnostics. Diff check was clean, all 17 paths were owned, and the excluded
files were byte/diff identical with blob hashes
`58ad821d31a5559225855fbb30a1566d374063e7` and
`b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f`. QA cleared the exact selector-1 /
A7A9 fixture: `AC0D3E09 mod 100 = 1`, probability 5, outcome MAKE. It also
cleared transactional/fail-closed loaders and state, geometry/sample/context,
matrices, points, routes/rattle, claimant, expiry, CPU defer, malformed/replay,
and evidence classifications.

## R2 revised-tip QA and terminal metadata boundary

QA fast-forwarded only its branch/worktree to
`8be0258e83369bce58d3a9eabedb4ef575127b25` and verified exact HEAD, tree,
parent, and merge-base, a clean worktree, and the exact three-doc identity.
The three docs were `README.md`, `PROOF-MANIFEST.md`, and `FAULT-LEDGER.md`.
All non-doc blobs were identical to the implementation candidate. The excluded
blob hashes remained `58ad821d31a5559225855fbb30a1566d374063e7` for
`src/tecmo_gameplay_scene.c` and `b6fc46a927f1a0cddedf7a965d3ebb4ad7d23b7f`
for `src/asset_pack/tecmo_asset_pack_source_map.c`. Initial runtime acceptance
carries without rerun. Revised-tip QA found one self-reference P2 only, resolved
by the terminal metadata revision.

The implementation commit and committed QA-lineage child are exact. The
terminal metadata tip SHA and its subsequent same-QA terminal-tip verification
are supplied in the authoritative Sol/master handoff because a commit cannot
contain its own object ID or a later audit result. This record does not assert
final QA acceptance.

## Commit and acceptance state

Implementation candidate commit: `24bdde9c87b1529d9ab83671bc8c60c1e136ceb1`,
parent/base `222d75cfafa9153db1eb44492bf557f11b1a9091`, tree
`367c14eb390f53a7b7a45c08d9ad1a02ab44d415`, message
`feat: complete R2 shot outcomes`. Committed QA-lineage child:
`8be0258e83369bce58d3a9eabedb4ef575127b25`, parent
`24bdde9c87b1529d9ab83671bc8c60c1e136ceb1`, tree
`5863c301ed00e8dedbc9e2af12a3c8b97ea876f3`, message
`docs: record R2 shot outcomes`, exactly 3 docs, 138 insertions, 30 deletions.
QA recorded clean worktree/index/range checks and exact docs-only scope.
The terminal metadata tip SHA and its subsequent same-QA terminal-tip
verification are supplied in the authoritative Sol/master handoff because a
commit cannot contain its own object ID or a later audit result. This record does
not assert final QA acceptance. The excluded files remain untouched, and
shared-file name/source-map follow-ups remain incomplete and deferred.
