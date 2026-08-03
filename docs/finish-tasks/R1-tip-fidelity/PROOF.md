# R1 TIP Fidelity — proof manifest and Sol visual review

Status: Sol accepted the source/ABI/pack/test review and the clean-commit
formal visual proof for implementation commit
`a37e10207455933be3930e90c55b10b669cb0ef3`. Final TIP closure remains pending
independent QA and Sol-branch integration. Sol's latest historical broad scene
wrapper run passed in DRAFT mode and produced ignored output at
`C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna\build\live-proof-20260803T205847090Z`;
the independent Sol rerun produced
`C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna\build\live-proof-20260803T204552541Z`.
The formal proof was then run on the clean implementation commit and no proof
artifacts are committed to the repository.

## Formal clean-commit proof — accepted

Invocation:

```powershell
.\tools\New-TipoffVisualProof.ps1 -ProjectRoot . -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
```

Result: `TIPOFF VISUAL PROOF PASS` on the first formal attempt, generated UTC
`2026-08-03T21:08:24.8165649Z`.

| Formal identity | Value |
|---|---|
| Root | `build\proof\tipoff-visual-orientation-a37e10207455` |
| Manifest | `proof-manifest.json`; SHA-256 `AAD8EF7AF9F075E5EA1F64B91C6F363A9E2959444D893F6D0C4A760368D11438` |
| Schema | `tecmo.tipoff-realtime-proof/2` |
| Repository commit | `a37e10207455933be3930e90c55b10b669cb0ef3` |
| Branch | `codex/r1-tip-fidelity-luna` |
| Clean-worktree gate | Required and passed |
| Summary SHA-256 | `1AA8C97B10E31368E28BAEE8232B3B892F2BC6D249F14295040ECD87D74FA78D` |
| Rev1 ROM SHA-256 | `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` |

| Build/pack identity | Value |
|---|---|
| Executable | `1,981,952` bytes; SHA-256 `0CA0CDAE6C837ACB2FC0C601D25830EEA1EA6FF74EEC7D0079CAFDEB99602257`; freshness gate passed |
| Formal build log | Warning-clean |
| Pack | `1,406,713` bytes; `86` entries; SHA-256 `A16D873CCBBDEBEFB19F101D34569F6F1CE280943A47221956D3B036BA89FEC4` |
| TPTI-2 payload | `7,680` bytes; FNV32 `28910BC1`; FNV64 `7EA1596E8DFAC0C1`; SHA-256 `C453848A33D6B29046D48ACDB44973D9A93234457C13F4150154F35DEA8F27FB` |
| Proof script SHA-256 | `A92F811E6EADFAEE3876E03FC2EF0725A728437CD8572CBE75F56F27240B2D93` |
| Checkpoint source SHA-256 | `C4586F9E36AAE773B47F74340115F85416BEEBDD7267FEDFAA927808D9CC4FDE` |

### Formal inventory and artifacts

- Inventory pass 1 contained exactly `65` PNGs named `0661..0725`.
- Inventory pass 2 contained exactly the same `65` PNGs plus one facing
  checkpoint. There were zero name, hash, or dimension mismatches; every PNG
  was `640x480`.
- There were `138` nonempty logs, each at least `196` bytes. No warning,
  fatal, exception, failed, failure, or `error:` matches occurred, and
  `proof-incomplete.marker` was absent. The worktree remained clean.
- Contact sheet: `3200x1592`, SHA-256
  `4D29B5323D21B0C0CEACE359AFE6AB55E5EE1A7B54C783629769426D31B5EB95`;
  full-resolution `640x480` 1:1 cells.
- Left edge sheet: `448x5108`, SHA-256
  `4785DD027E8180A145517C824BC4AABEEA064EA39273450E316B3CC39BDB051A`.
- Right edge sheet: `448x5108`, SHA-256
  `7E8FF07AB0CF4D1FC3EDDCF582A8F2F82F359EAE03B5729593ADAF62E3B5BBB0`.
  All `65` frames were represented and outside-active-view host margins were
  black.
- Facing checkpoint: `640x480`, SHA-256
  `DDE21802E85DD14AC85F8792CBB9694C0833E5DC103A1C567891B1501F6FA783`,
  deterministic across two renders.

### Presentation-only video

The MP4 is presentation-only and never acceptance proof: `98,535` bytes,
SHA-256 `4215BBF4733E71D2FFE8EC2D6C16DDF60AF187B7FE1585141320B34BEB8D4C20`,
`640x480`, `65` decoded/stored frames, average and real frame rate
`39375000/655171`, duration `1.081552s` versus expected
`1.0815521269841271s`. ffmpeg SHA-256 is
`D1E2A156261ECC675081943197A85F08F2868784A0AF499171EDE89353EDAD31`; ffprobe
SHA-256 is `70872C3FFBC43D0B2C570F9837F54D6E9A832F4CA25463E9735B6A3EC0621478`;
both are 8.1 full builds.

### Representative frame hashes

| Logical frame | SHA-256 |
|---:|---|
| 661 | `D35724DAD5420A3C09BDA51DEE8D2319CB33F88D103C77E3BD2BB84A053CDED6` |
| 662 | `84377B141641CB526B8DA0D959BB0771363759FE2EACBF90859A2C82C74E68BF` |
| 683 | `18550DA41BB81E5270B46BB92CA8EBCB641961A2923D04ECBEA7FFEBF81D9EF8` |
| 687 | `33A3F3254F9928DBE81020FDF743E09BB9DC8B3E11B86370276F5C57FA989C2D` |
| 696 | `50B1264222C7797F2648414E70A13B232558D5BA03EEC378FD51CB1778288031` |
| 720 | `05AECDA2CD1CF9E0444CFC030C6F8A2C7E27128FCA53A84E4669B395E72F4607` |
| 721 | `6CFC130D0A472BCB7877F1B48037BC1DD2DC7BB16D0460692060C699BE2C917A` |
| 725 | `80419403637392438810992405944F50D1F9363744784A6F038D5F4F04613723` |

### Runtime and visual acceptance observations

- The physical-X fast pulse occurs only at logical `662`; Away human sample
  is raw frame `0`/error `0`. Home unassigned automatic sampling occurs at
  logical `683`, raw frame `21`/error `11`.
- Across all `60` contest frames, both visible jumpers remain anchor-ordered
  and face inward; camera X remains `256`. Contest-frame sampling caps at `30`.
- Ball X remains centered through logical `691`, then moves exactly one pixel
  per update left to the `8px` cap. Logical `721` is LIVE with Away possession,
  left direction, and holder slot `0` enforced by checkpoint preflight;
  continuity remains valid through `725`.
- Sol personally inspected full-resolution frames `661`, `662`, `683`, `687`,
  `696`, `720`, `721`, and `725`, the contact sheet, both edge sheets, and the
  facing checkpoint. Court, HUD, players, jumpers, ball, landing, and the
  half-court LIVE handoff were coherent; no clipping, corruption, snap, or
  host-margin leakage was observed.

### Original-reference comparison and classification

Sol personally inspected private, inventoried `256x224` original-reference
contact sheets: automatic
`4E4E4D9E9E33CE245E416B84F89B0F18FEA394C2741413AF1496CCE67E6717A5`,
simultaneous
`01ECAE0D235E1433263CEBC4F828A31700A690C2AB724DF8BE03B6E195125894`, and
human-vs-CPU one-down
`630BC1CA5A0E33AD28257CAB22B5D8B0C5DE3213BC4E2EB90763DEF390E6C29C`.
The source original shows a close-up ball/arms cutaway and a different longer
post-claim path. Native preserves the accepted `640x480` center-court visible
arc and frame-721 handoff. The visual trajectory/camera composition is native
approximate; the unproven TTDT/`$7C48` trajectory and original tie settlement,
selector-to-team, and selector-to-receiver mappings remain incomplete. Frame
721 is not claimed as ROM-exact.

## Proof contract and formal result

The formal proof passed only after requiring all of the following to be present
and internally consistent:

- complete contiguous full-resolution frame sequences for every requested checkpoint;
- exact video/frame counts and cadence;
- manifest SHA256 values for frames, contact sheets, and video;
- base/final binary SHA, ordinary TPTI-2 payload FNV32/FNV64, ROM identity, input route, resolution, and timestamps;
- mapper/state evidence for every claimed frame-721 handoff;
- clean tracked-tree status and clean command/log capture;
- original-reference comparison for unchanged and intentionally changed checkpoints;
- no missing frame, log, video, metadata, stale output, wrong dimension, cadence, or hash accepted.

The proof script now derives the home automatic logical sample frame rather than hardcoding it:

`$HomeSampleLogicalFrame = $ProofFirstFrame + $HomeAutomaticSampleFrame + 1`

With the accepted values (`661 + 21 + 1`), the runtime assertion requires `683`. The raw automatic sample remains frame `21` and its bounded capture error remains `11`; this distinction is part of the proof contract.

The earlier broad-wrapper DRAFT manifest self-validated with `254` inventory
artifacts and `255` files. That historical DRAFT remains diagnostic evidence
only and is distinct from the accepted clean-commit formal proof above.

The worker's post-`43ab7a7` flow-fixture correction and the earlier
`f951098` live-proof fixture correction both preserve the real 721-update
handoff. The worker DRAFT contains `14` frames, `2` native videos, and a
`1920x1440` contact sheet; the independent Sol DRAFT has the same counts and
dimensions. Neither DRAFT is the formal proof.

The latest personal Sol gate snapshot also records warning-clean console+Win32
build, explicit console-flow and GUI/console production smoke, and passing
focused pre-tip and broad scene gates. Its historical DRAFT root is
`build/live-proof-20260803T205847090Z`.

## Expected changed visual checkpoints

The following eight checkpoints are the only expected pixel changes against the read-only TPTI-1 baseline. Each was independently rendered twice for base and current builds. Diffs are confined to the ball sprite bounds; players, court, HUD, and the accepted visible arc are unchanged.

| Checkpoint | Final SHA256 | Diff pixels | Bounds |
|---|---|---:|---|
| pretip680 | `A707A2C6D82DD2E2B2BAC3B5ABC7F790A95E13CC5BC825EEDEA330A5A9C1445C` | 136 | x314..331, y168..183 |
| pretip690 | `5272AFDC75E669C16B537D93D4CA96BCBC7DF283C835EE99D353CF31F5D6E43E` | 196 | x314..341, y188..203 |
| pretip696 | `4C4861E2992E0A431560B2F7AA0D7EFDDE47B6DEE96E772F931D93401EEDF86C` | 112 | x324..341, y202..215 |
| proof676 | `32F27C33BF01C9D07A8DAAA748FC05875F7DF3E6D82595E883B5DFE747335ED4` | 192 | x300..325, y160..175 |
| proof686 | `AD2AB7FCB8FBE1637C7EECA3D7C3FA14D1B57A35041778714EA471574E275D2E` | 176 | x298..325, y180..195 |
| proof687 | `33A3F3254F9928DBE81020FDF743E09BB9DC8B3E11B86370276F5C57FA989C2D` | 176 | x298..325, y182..197 |
| proof696 | `50B1264222C7797F2648414E70A13B232558D5BA03EEC378FD51CB1778288031` | 156 | x298..315, y200..215 |
| proof697 | `EAFD4442CDC8C7DB0CEB028BA5A3BA38B7A846AFCC7CC1A1C98DED7994626690` | 156 | x298..313, y202..217 |

Unchanged comparisons include pretip675, pretip712, proof668, proof712, and
the card/toss/LIVE checkpoint set.

## Sol visual placeholders

- [x] Sol confirmed the 60-update crouch/rise/apex/fall/land arc and both visible jumpers.
- [x] Sol confirmed no one-update ball-X snap, monotonic bounded travel, and the eight ball-only diffs above.
- [x] Sol confirmed frame-721 holder slot `0/5`, LIVE lineup/link/matchup state, clock/music/camera, and possession invariants.
- [x] Sol confirmed all generated frame/video/contact-sheet dimensions, counts, hashes, and metadata.
- [x] Sol recorded the formal proof output path and command facts above. Independent QA and Sol-branch integration remain pending.
