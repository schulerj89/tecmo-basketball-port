# Tests, builds, and proof status

Sol accepted implementation commit
`a37e10207455933be3930e90c55b10b669cb0ef3` and its clean-commit formal proof.
The post-rescope gates all exited `0`: `build.ps1` built console+Win32 with no
warning lines; `Run-Win32LaunchSmokeTest.ps1` passed explicit console flow and
GUI/console production smoke; `Run-GameplayPreTipTests.ps1` passed; and
`Run-GameplaySceneTests.ps1 -Build` passed with historical LIVE PROOF DRAFT at
`build/live-proof-20260803T205847090Z`. Independent QA and Sol-branch
integration remain pending.

## Focused commands passed

All commands below were run from the worker worktree against the built
`build\tecmo.assetpack` unless noted.

| Command | Result |
|---|---|
| `\.\build.ps1` | Exit 0; production native and Win32 targets built. Final build output had no warning lines. The existing script updated the desktop shortcut as an external side effect. |
| `\.\tools\Run-Win32LaunchSmokeTest.ps1 -ProjectRoot . -DecompRoot C:\Users\joshs\Projects\disassem\tecmo-basketball-decompilation` | Exit 0 after the `43ab7a7` flow-fixture correction; explicit console flow, GUI/console subsystem, project-root argument, working-directory, icon, roster-independent startup, window lifetime, and clean shutdown checks passed. |
| `\.\build\tecmo_port.exe --gameplay-pretip-test .\build\tecmo.assetpack` | `TPTI-2 pre-tip self-test passed` |
| `\.\build\tecmo_port.exe --gameplay-pretip-human-checkpoint .\build\tecmo.assetpack` | `TPTI-2 human checkpoint PASS frame=721 late-sample=29 win32-X=assigned-Away-frame-0 fast-X=one-frame B-unmapped` |
| `\.\build\tecmo_port.exe --gameplay-scene-test .\build\tecmo.assetpack` | `GAMEPLAY SCENE SELF TEST PASS` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-GameplayPreTipTests.ps1 -ProjectRoot . -RomPath <canonical Rev1 ROM>` | `TPTI-2 PRE-TIP TEST PASS`; canonical/revision/FNV32+64/source-map, same-pack TGJS-2, stale-TPTI-1, false-friend, non-hook, ordering, E56E-count, overlap/bounds/padding, scene integration, deterministic renders, and reference comparison all passed. |
| `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-GameplaySceneTests.ps1 -ProjectRoot . -RomPath <canonical Rev1 ROM> -Build` | Exit 0: `GAMEPLAY SCENE TEST PASS` and `LIVE PROOF DRAFT`; 14 contiguous proof frames, two native videos, and a 1920x1440 contact sheet were produced under ignored `build\live-proof-20260803T205139679Z`. |

The focused harness rebuilt a scratch pack from the canonical ROM and kept all
generated files under ignored `build\gameplay-pretip-tests`. The exact
`flow_finish_gameplay_pretip` fixture correction keeps the existing 721
iterations, recomputes P1 cancel only during `JUMP_CONTEST`, keeps P2 neutral,
and preserves both callers and post-handoff assertions.

Sol independently reran `Run-GameplaySceneTests.ps1 -Build` with exit `0`:
`GAMEPLAY SCENE TEST PASS`, DRAFT root
`build/live-proof-20260803T204552541Z`, `14` frames, `2` videos, and a
`1920x1440` contact sheet. This independent result is separate from the
worker's `205139679Z` DRAFT.

The latest personal Sol broad result supersedes neither earlier diagnostic
roots nor their recorded recovery history: it is the post-flow DRAFT root
`build/live-proof-20260803T205847090Z` and remains non-formal proof.

## Formal proof result

The first formal invocation was:

```powershell
.\tools\New-TipoffVisualProof.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
```

It returned `TIPOFF VISUAL PROOF PASS` on the first attempt at UTC
`2026-08-03T21:08:24.8165649Z`. Root:
`build\proof\tipoff-visual-orientation-a37e10207455`. Manifest schema:
`tecmo.tipoff-realtime-proof/2`; manifest SHA-256
`AAD8EF7AF9F075E5EA1F64B91C6F363A9E2959444D893F6D0C4A760368D11438`; summary
SHA-256 `1AA8C97B10E31368E28BAEE8232B3B892F2BC6D249F14295040ECD87D74FA78D`.
The manifest records exact commit `a37e10207455933be3930e90c55b10b669cb0ef3`,
branch `codex/r1-tip-fidelity-luna`, and a passed clean-worktree gate. Full
inventory, artifact hashes, runtime observations, and visual/reference review
are recorded in `PROOF.md`.

## Named runtime and source negatives covered

The C self-test and scene suites retain existing tests and add/recheck:

- raw claim underflow (`ball < jumper`), difference `$39` accepted and `$3A`
  rejected, strict automatic threshold equality rejection;
- sampled error range `0..11`, unsampled sentinel `12`, and sample-frame versus
  error separation for automatic frames 20/21/22;
- both-automatic ages 20/22, one-down age 21, equal same-frame human deferral,
  no-input stall, bounded helper/no-progress termination, and late frame-29
  human plus automatic frame-721 handoff;
- per-jumper raw claim units versus genuine Q8 diagnostic units, commit-origin
  altitude, commit+1/commit+30/LIVE altitude, and preserved scene arc landmarks;
- smooth/capped/monotonic ball X with no pre-resolution snap;
- resolved/deferred state timing, wrong/not-ready claimant, early resolved or
  deferred state, completed-ready unresolved state, and pre-contest automatic
  request mutation, all transactional;
- threshold/claim-limit cache mutations, TGJS-2 cross-pack/mutation rejection,
  stale TPTI-1 payload/header, source alias/bounds, full padding 6656..7007,
  B04 overlap, 8642 false friend, A2D1 non-hook, E537/E542 ordering, recurring
  E56E count, and source-role/provenance negatives;
- jumper claimant versus receiver/holder false friend, holder slot 0/5, LIVE
  lineup/link/matchup preservation, and skip-PRETIP valid-state normalization;
- all existing scene/render-contract suites remain in the orchestrator.

## Deterministic visual checkpoint ledger

The new exact claim gate changes only the early ball sprite position. Two
passes of the read-only Rev1/base executable and two passes of the current
TPTI-2 executable were compared for each changed checkpoint. Old hashes match
the base contract, new hashes are deterministic, and every pixel difference
is inside the ball sprite bounds; no player, court, HUD, or arc pixels differ.

| Checkpoint | TPTI-1/base hash | TPTI-2 hash | Pixel diff | Bounds |
|---|---|---|---:|---|
| pretip 680 | `C323C49D63D9615C84F2F3743C79FFF32A1FE15479848899BC2729A057688CB7` | `A707A2C6D82DD2E2B2BAC3B5ABC7F790A95E13CC5BC825EEDEA330A5A9C1445C` | 136 | `x=314..331,y=168..183` |
| pretip 690 | `EF7A09D5D37B098B346A1CBF53D4F822ADD23B53E0D0283143E39F1CF2399CEE` | `5272AFDC75E669C16B537D93D4CA96BCBC7DF283C835EE99D353CF31F5D6E43E` | 196 | `x=314..341,y=188..203` |
| pretip 696 | `3A7048CF6BB7E8ADD411703D17C9B287EAEE83F436825C7BEAC111EEC84F4F6A` | `4C4861E2992E0A431560B2F7AA0D7EFDDE47B6DEE96E772F931D93401EEDF86C` | 112 | `x=324..341,y=202..215` |
| proof 676 | `F90E2328044DC6F56D705E654CAEAE4CC80D6330D12EB846B14E7931DB703053` | `32F27C33BF01C9D07A8DAAA748FC05875F7DF3E6D82595E883B5DFE747335ED4` | 192 | `x=300..325,y=160..175` |
| proof 686 | `5915FE0073906193E619B3CC516D25C1B8D18F05FD84C6CFC103A90DCCA68059` | `AD2AB7FCB8FBE1637C7EECA3D7C3FA14D1B57A35041778714EA471574E275D2E` | 176 | `x=298..325,y=180..195` |
| proof 687 | `B2C594027F608B436099F31BDDF2D40D819385B7678C148BC147377D77EE0BCD` | `33A3F3254F9928DBE81020FDF743E09BB9DC8B3E11B86370276F5C57FA989C2D` | 176 | `x=298..325,y=182..197` |
| proof 696 | `BA2C0FE1DBE66F5BC60E4222BE860E1AC2A156388F36C20DD707607952188984` | `50B1264222C7797F2648414E70A13B232558D5BA03EEC378FD51CB1778288031` | 156 | `x=298..315,y=200..215` |
| proof 697 | `5B70E59F52D1FE542B30F0D61D2BF2C6604A227DB2897D89B4C3A33334A7F014` | `EAFD4442CDC8C7DB0CEB028BA5A3BA38B7A846AFCC7CC1A1C98DED7994626690` | 156 | `x=298..313,y=202..217` |

Unaffected checkpoints verified unchanged include pretip 675 and 712, proof
668 and 712, plus all card/toss/LIVE checkpoints in the focused harness.

## Remaining closure gates

- Formal `New-TipoffVisualProof.ps1` passed on the first attempt at the clean
  implementation commit. Its derived `HomeSampleLogicalFrame = 661 + 21 + 1
  = 683` guard, complete frame/log inventories, dimensions, hashes, cadence,
  and metadata all passed.
- Broad `Run-GameplaySceneTests.ps1` passed after both authorized fixture-only
  corrections. The live-proof fixture and flow fixture hold/recompute only
  P1/Away input during `JUMP_CONTEST`, clear it otherwise, and preserve the
  real 721-update handoff. The wrapper was run without `-RequirePass`, so its
  manifest is intentionally `DRAFT`.
- Production Win32 launch smoke and formal visual inspection passed. Independent
  QA and Sol-branch integration remain pending. No test suite was deleted or
  weakened.
