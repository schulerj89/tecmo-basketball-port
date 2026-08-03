# Integration evidence

## Terminal personal proof identity

Accepted ignored proof root:

`C:\Users\joshs\Projects\tecmo-basketball-port-r1a-cpu-live-integration-qa-sol\build\r1a-integration-qa-final-20260803-f`

| Artifact | Bytes/dimensions | SHA-256 |
|---|---:|---|
| `PROOF-MANIFEST.json` | 575510 | `F72DC259DFFD6B95B560088D591A059DE9F572ED30D2FA70E889A32977273F43` |
| `contact-repeat-1.png` | 163089 / 1920x1440 | `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C` |
| `contact-repeat-2.png` | 163089 / 1920x1440 | `F8380481C46C9836773F8970775F785B5FE1D0FE8E059DA066E0D6D37C8F8A9C` |
| `native-repeat-1.mp4` | 37915 / 640x480 | `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595` |
| `native-repeat-2.mp4` | 37915 / 640x480 | `B8653E4D0DB44AEA437BE9BFB8C545D38B82821809195B956807B5204E087595` |
| preserved proof asset pack | 1401618 | `695EEB2D0101C5422B01790BD8D6B2A7607E758F396F429C2D2424AC6A26DE07` |

The manifest schema is `tecmo.live-proof-manifest/TGLP-1`, status is `PASS`,
`require_pass=true`, `build_requested=true`, and `build_warning_clean=true`.
It records base `ad0f005673692b04772bce3c3b4d3ac4b2624731`, current/final
`351f446dddc96c34c838c5a9642a0be9d7f1411e`, and the R1A branch. That merge
has exact parents `f98fea320bf2340e0c6c9b226cfe6caa63196dd7` and
`bcacd5b6963f4db1a92c8db9b9770413505a0e98`; `f98fea3...` in turn has exact
parents `222d75cf...` and `dd096cb...`. Proof time was
`2026-08-03T19:31:37.2029425Z` through
`2026-08-03T19:32:09.3616972Z`.

The intermediate reconciled proof at
`build\r1a-integration-qa-merged-20260803-d` remains valid evidence at
`f98fea3...`: its 576034-byte manifest SHA-256 is
`99BAE86564963189A5A93B4975BA8B791AB2CF2190E964BD9932004BE770F747`.
It completed before accepted R4A advanced current main. Terminal acceptance
uses the later `351f446...` proof above.

The earlier frozen-candidate proof at
`build\r1a-integration-qa-personal-20260803-b` remains valid time-scoped
evidence at `222d75cf...`: its 577082-byte manifest SHA-256 is
`8E245A62676832F2D75E7BD930682CEA1ED51D80064860B90178691E5251F0C4`,
and it completed at `2026-08-03T18:21:09.9305271Z`, before `main` advanced to
R3A. Terminal acceptance uses the later merged-tip proof above.

## Complete artifact audit

- `189/189` required logs existed, were nonempty, and matched exact manifest
  byte counts and SHA-256 values.
- `254/254` inventory paths were unique, root-contained, present, and matched
  exact byte counts and SHA-256 values.
- The proof tree had `255` files: all 254 inventory files plus the manifest,
  which intentionally cannot inventory its own final hash. There were no
  unlisted proof artifacts.
- Extension inventory was 2 asset packs, 12 JSON, 14 JSONL, 193 logs, 2 MP4,
  30 PNG, and 2 TXT. No ROM, ASM, decompilation, save state, private raw capture,
  or proprietary artifact was present.
- All `13` negative regressions were recorded: dirty/wrong-branch/non-ancestral
  RequirePass; stale ROM/pack/contact/video-rate/video-count/video-time-base;
  missing/malformed/cross-pack input; and missing/byte-mismatched/SHA-mismatched
  inventory.

## Deterministic native media

Both repeats stored seven numbered full-resolution 640x480 PNGs in this order:

1. `pretip-start`
2. `live-handoff`
3. `human-movement`
4. `offensive-pass`
5. `defensive-switch`
6. `cpu-target-deferred`
7. `shot-path`

Every paired PNG and state JSON matched by SHA-256; the state-level frame FNV
also matched. The two contact sheets and the two videos are byte-identical.
Independent `ffprobe` found 640x480, `r_frame_rate=avg_frame_rate` of
`39375000/655171`, `time_base=1/39375000`, and exactly `7/7`
`nb_frames/nb_read_frames` in each MP4. Independent `ffmpeg` frame decoding
matched the manifest decoded lists; both list files have SHA-256
`6FA0AA43130E1EFF92986485EC6305ABE9A86781968FEC24371FA45616F13E9B`.

Personal inspection used the repository screenshot QA checklist on both
intermediate contact sheets and all seven repeat-1 numbered frames at original
detail. The terminal contact/frame/video hashes are byte-identical to those
inspected artifacts, and the terminal contact was reopened directly at
original detail. The native sequence shows a centered PRESEASON title followed by coherent
Hawks/Celtics court frames. The score/clock/player HUD is readable, ten actors,
court, crowd, basket, ball presentation, and camera framing remain coherent,
the pass/switch name changes are visible, and the last frame reaches 3:59 with
the shot path active. No blank/corrupt frame, HUD overlap, clipped court,
missing actor, broken texture/palette, or camera defect was observed. Because
the repeat-2 files are hash-identical, that inspection covers the identical
second raster/video sequence as well.

## Original-reference provenance and honest comparison

The immutable accepted CPU formal manifest is `128484` bytes with SHA-256
`E7C9E6C9210D398DADC82715779A1389DF881643D109A0FDB091EBAFA523254A`.
It identifies:

- canonical Rev1 ROM SHA-256
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`;
- private FCEUX executable SHA-256
  `F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`;
- two deterministic emulator runs, no RAM writes, cheats, or savestates;
- a real 120-frame running-clock window beginning after live handoff, with 12
  256x224 PNG observations per run and a separate 256x240 video contract;
- equal original frame/contact hashes across runs. The 768x896 contact-sheet
  SHA-256 is
  `2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.

The CPU manifest records final SHA `8be7a9f9a11d43e68b090a98af122758885931fd`.
The successor range through accepted CPU terminal `ad0f005...` is docs-only;
the independently checked CPU source, map, runner, TGAI, ROM, FCEUX, and
artifact hashes are unchanged. This is explicitly time-scoped CPU evidence,
not a claim that the proof was generated at `ad0f005...`.

Personal inspection of the original contact sheet and frames 1, 6, and 12
confirmed coherent Bulls/Celtics running-clock gameplay. This evidence is
reference-only: it differs from the native proof in teams (Bulls versus
Hawks), 256x224 raster versus 640x480 render, camera/view, continuous capture
versus selected event fixtures, and schedule/timing. No pixel or behavior
parity is claimed, and no original AVI is required or claimed.

## Event/state invariant audit

All 14 native states had schema-valid 640x480 output, ten actors in slots
`0..9`, valid LIVE foundation state, formation index 30, selected roster arrays
away `[5,6,10,11,0]` and home `[11,10,6,5,1]`, and fixed links
`[5,6,7,8,9,0,1,2,3,4]`.

The first repeat showed:

- pre-tip: frame 0, no holder, first sync pending, static seeds 4/9;
- LIVE handoff: frame 721, holder/primary 0, defender 5, sync serial 1;
- human movement: one-update TGMO continuity and source-target state;
- offensive pass: holder/primary 1, defender 6, sync serial 2;
- defensive switch: home possession, holder 5, away controller switched to 2,
  primary/defender 5/0;
- deferred CPU target: cleared controlled actors, preserved explicit deferred
  source effect without fabricating a movement target;
- shot path: action serial 1, close-shot kind 2, actor 0, exactly one accepted
  request, supported playback true, deferred false.

## CPU-to-LIVE source audit

The R3A side changed nine season/team-data paths, while the frozen R1A side
changed 43 paths; exact normalized intersection was zero. A three-way
`git merge-tree` from `6d8f9c7...` was clean. After accepted R4A advanced
current main, its side changed 18 audio/docs paths from `dd096cb...`; the R1A
side still changed 43, with exact intersection zero. Git's native merge-tree
returned exit 0. The terminal tree then passed the full build, CPU, movement,
flow, season, music/frontend/gameplay audio, scene-proof, and Win32 gates, so
this conclusion is signed at `351f446...`, not inferred from either earlier
proof.

Key personally inspected production boundaries:

- `src/tecmo_game.c`: production preseason and season launchers call the
  by-value starter validator before scene launch and mode transition.
- `src/tecmo_gameplay_scene.c`: launch validates/normalizes starter arrays,
  binds each stable actor slot to the selected roster record, initializes the
  LIVE foundation, and records the legacy origin internally rather than
  accepting a caller-set legacy bit.
- `src/tecmo_gameplay_live_foundation.c`: initialize, synchronize, play step,
  and shot predicate operate on candidates, validate the complete source
  contract, and write outputs only on success. Holder/orientation/controller
  transitions invalidate stale target/direction metadata. Observation serials
  wrap deliberately instead of failing a long session.
- `src/tecmo_gameplay_scene_actors.c`: all ten CPU decisions consume one
  immutable post-human snapshot; CPU/foundation/actor/ball/shot state commits
  only after all validations. Unproven/no-target effects are inert, outward
  edge/corner directions are explicitly deferred, and no retired formation
  fallback fabricates a target.
- `src/tecmo_gameplay_scene_validation.c`: fail-closed ownership ties actor
  slots to the bound selected roster, controller team to controlled holder,
  and LIVE primary/defender/possession/orientation to the scene.
- `src/tecmo_gameplay_scene_test_state_flow.c`: malformed lineup and complete
  scene rollback tests, wrong-team holder and serial-wrap transactions,
  pass/switch/handoff synchronization, 46/48 formation rejection, edge/corner
  inert behavior, shot suppression/defer/playback classification, and the
  120-update running-clock regression all execute in the full scene gate.
- `src/tecmo_gameplay_cpu_steering.c`: exact header/payload/source/corpus checks
  and same-pack TGMO validation fail closed before availability. The LIVE slice
  did not modify this accepted CPU core.
- `CMakeLists.txt` and `build.ps1`: both list
  `tecmo_gameplay_live_foundation.c` and `tecmo_gameplay_live_proof.c` beside
  the CPU unit. `git diff --check` passed.

## Original ASM/source evidence index

| Bank/address/table | Identity | Accepted meaning |
|---|---|---|
| Bank06 `$81F7-$82D3` | 221 bytes / FNV `23BB7271` | exact ten-actor state-4 loop |
| Bank06 `$87AE-$88AF` | 258 / `F866B06C` | exact reference-direction path |
| Bank06 `$88DA-$8A95` | 444 / `9616E586` | exact target-direction octant path |
| Bank06 `$8B90-$8BE0` | 81 / `9AD2BA91` | exact fetch/dispatch boundary |
| Bank06 `$8BE1-$9237` | 1623 / `344298FE` | exact 24 handler-entry bytes |
| Bank06 `$9280-$9329` | 170 / `C82E6853` | exact target apply/common tail |
| Bank06 `$938B-$9620` | 662 / `47818A62` | 46 source-pinned formation starts; 46/47 reject |
| fixed `$C006-$C008` | 3 / `14B2472E` | exact trampoline |
| fixed `$CBE0-$CBF6` | 23 / `41C5B5C8` | exact five-byte reader |
| Bank04 `$9F2E-$AC75` | 3400 / `71331A96` | exact 680 aligned command records |
| Bank04 `$AC76-$ACF0` | SHA `AA296CBBF2269130F13C8D6983D8974517710B9A0641A6E9770E50438E07A20A` | exact bytes; bounded startup semantics |
| Bank04 `$ACD9-$ACE3` | SHA `4761CF44148247C6B96046AE8FA2A9B899BCDD2A3BCB2AD61EFA3BEBBAD9414D` | exact fixed-link producer |
| Bank04 `$ADD6-$ADDF` | SHA `710E206A0E4A6919A8323E87F40D891B73F8FBC204EA286CE75DE5ED75440155` | exact links `05 06 07 08 09 00 01 02 03 04` |
| Bank05 `$96B6-$9708`, table `$9709-$970A` | SHA `307715F21D95CEEB5033EDD4DD77BE665215E5F2993663D9AB81B17A50D40A48`, bytes `00 80` | exact route mechanics; route labels inferred |
| Bank06 `$8374-$84B6` | SHA `0E34FEFAC7DC767B0A0286FD3BD7A849A2495D24F003A18DABFB186F9BB4981F` | exact shot gates/timing bytes; outcome deferred |
| Bank06 `$B081-$B365` | SHA `AAA9670DA5942FA2614F925A266674893A352BB2DB3A8F4158F61C8AE891AE36` | exact identity only; dynamic selection deferred |
| Bank03 `$8FC2-$9102` | 321 bytes / SHA `FA3B396D01581451717CEB44A0F5628560FC664191E8F15F5843B0EAB316A9F5` | exact selected-starter staging |
| Bank04 `$ADE0-$ADF3` | 20 bytes / SHA `B4CC98CF95216620E6DAAB21C71BC1D9A679AFE9BB8BE5DC455A239E07640A3B` | exact staging into `$7B2E-$7B37` |
| Bank04 `$AC76-$ADDF` | 362 bytes / SHA `E123614333986D9D5084678C9AE32DD3A1A28ABF52F6D6265FE749FC0070C6E0` | exact setup neighborhood |

Runtime address distinctions remain explicit: `$8B90` fetch, `$8B9F` reader
call, `$8BA2` copied opcode, `$8BAE` indirect dispatch, `$8FD9` five-byte
advance, `$8FE8` rewind, `$8BB1/$8BC9` static table anchors, and `$8BE1`
opcode-22 handler. Exact addresses do not elevate inferred semantic labels.

The exact 24-entry handler table in opcode order is:

```text
$90E0 $934B $9280 $905E $8FFA $8F92 $8F2D $8F12
$8ED7 $8FC5 $8CD0 $8C40 $8E4F $9125 $9146 $9172
$9085 $8C1A $8C1A $8C1A $9032 $8BF6 $8BE1 $8F72
```
