# Evidence and reconciliation

## Initial frozen audit gate

- Worktree:
  `C:/Users/joshs/Projects/tecmo-basketball-port-r2-gameplay-presentation-sol`
- Branch: `codex/r2-gameplay-presentation-sol`
- HEAD/base: `ed060720a98b790f98591af363a490a0e0816018`
- Local `main`, cached `origin/main`, and live remote `main` were rechecked at
  that same object.
- The worktree and branch registry are unique and the status is clean.
- Root `AGENTS.md` (1,458 lines) and `PORTING.md` (1,687 lines) were read fully.

No build, test, game run, render, emulator run, capture, asset generation, or
proof generation was performed during that initial read-only audit phase.

## Accepted implementation evidence

The signed implementation candidate is
`4cb0c43bcd4c7ca111c996b3788e1bd00a734424`, tree
`3bd5b4874eca46ff7ad771041e96946c3b08f233`, sole parent
`ed060720a98b790f98591af363a490a0e0816018`. Its Good SSH signature is for
`jaystar524@gmail.com` with RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. The diff contains only:

- modified `src/tecmo_cli_render_gameplay_checkpoint.c` inside the three
  authorized symbols; and
- new `tools/Run-GameplayPresentationTests.ps1`.

The focused runner's personal proof is under ignored build output
`build/gameplay-layup-proof-personal-4cb0c43`. Its manifest SHA-256 is
`90EFB0AAAF55BB9853B8ED52A1E476701E388F389FD895922A4111E9BF1A91DD`.
The manifest-recorded contact sheet
`gameplay-layup-contact-sheet.png` is 2560x2400 with SHA-256
`85995E1654354BFFF874AEA8510F91FD6E0AB1BCECD49C5138EBC116FB9B6A6C`.

The proof establishes:

- 17 pass-one and 17 pass-two PNGs at exactly 640x480;
- byte-stable PNG and state equality between passes;
- 17 distinct pass-one frame hashes, plus nonblack and color-count sentinels;
- exact 3,144-byte TGCS entry identity, header size 256, FNV
  `DACDC976`, and variant-2 phase schedule
  `0,1,2,3,3,4,4,4,5,5,5,5,5,5,5,5`;
- active `shot=layup` for frames 1 through 16 and terminal
  `shot=none` at frame 17;
- the observed terminal state
  `gameplay-state frame=17 shot=none phase=live score=2/0 clock=3:00 period=1 overtime=0 shot-clock=24 pretip=live phase-frame=0 violation=NONE`;
- transactional rejection of frame 0, frame 18, malformed, empty, signed, and
  leading-zero forms; and
- branch, HEAD, clean status, mode, frame, dimensions, and SHA-256 in the
  manifest.

The terminal score is a production settlement result. The fixture does not
write score, shot kind, close-shot variant, pose, schedule, outcome, claimant,
or settlement state.

## Personal product and visual QA

- Canonical `build.ps1` completed both `tecmo_port.exe` and
  `tecmo_port_game.exe` with zero warning text.
- A fresh Visual Studio bundled CMake x64 configure and Release build completed
  both native targets with zero warning text.
- `Run-GameplaySceneTests.ps1` passed its full deterministic scene/provenance
  matrix and produced
  `build/gameplay-scene-proof-personal-4cb0c43/PROOF-MANIFEST.json`.
- Focused close-shot, dunk-cutaway, shot-resolution, camera-projection,
  court-orientation, court, violation-referee, penalty, and gameplay-asset
  suites all passed.
- Sol inspected frames 1 through 17 individually at original 640x480
  resolution and inspected the 2560x2400 contact sheet. HUD and camera remained
  stable; actor, pose, and ball changes were readable; the hoop and court edge
  were intentional; frame 17 returned cleanly to live play; and no corrupt
  sprite, crop, tear, overlay collision, or unexplained clipping was found.

The broad scene manifest remains explicitly DRAFT/PENDING original-reference
evidence. Passing native builds and deterministic native render hashes does not
promote it to emulator or cycle parity.

## Independent terminal disposition

The sole terminal read-only Luna independently verified the commit identity,
Good signature, path/function boundary, parser transaction, fixture writes,
production layup/variant-2 selection, runner logic, proof hashes, saved images,
and evidence labels. Its exact disposition was:

- PASS;
- P0: none;
- P1: none;
- P2: none;
- P3: none;
- actionable issues: none.

Its only artifact note was that the supplied contact hash belongs to
`gameplay-layup-contact-sheet.png`; a literal `contact-sheet.png` is not part of
the proof. That is not a proof failure.

## Canonical private research input

- Rev1 ROM length: 393,232 bytes.
- SHA-256:
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

ROM, decompilation, captures, saves, and historical proof remained read-only.
No proprietary bytes, ASM payload, screenshots, or decoded tables are committed.

## Evidence matrix

| Area | Current bounded evidence | Classification and limit |
| --- | --- | --- |
| Court/camera | TGCT full-court coordinates and 33-column viewport; TGCP init/follow/settle/project routines; `src/tecmo_gameplay_scene_court.c`, `src/tecmo_gameplay_camera.c`, and `src/tecmo_gameplay_scene_test_render_contract.c:922-1490` | Exact-source-pinned primitives and native-faithful integration. No PPU-cycle or generalized camera parity claim. |
| Movement/poses | TGMO clamp, step, pose-index, and facing primitives; `src/tecmo_gameplay_movement.c:540-810`, `src/tecmo_gameplay_scene_actors.c:407-596` | Exact locomotion/pose primitives; caller, spacing, links, CPU lifecycle, pass action, and defense animation remain approximate/incomplete. |
| HUD/overlays | Strict THUD parser/font/team marks; two fixed native rows, black backing, blank clearing, cutaway suppression; `src/tecmo_gameplay_hud.c:324+`, `src/tecmo_gameplay_scene_render.c:304-482` | Source-pinned assets and native-faithful named composition. Fixed columns, score caps, CPU-label fallback, and broad overlay parity are native adapters/incomplete. |
| Clipping/edges | TGCP visibility sentinel and boundary tests at screen X 255, -1, and 256; guarded 33-column fine-scroll margins; `src/tecmo_nes_video.c:38-62`, render contract at `1137-1395,2589-2675` | Native framebuffer clipping is safe. Original sprite overflow, partial-tile/OAM behavior, and live actor priority at edges are unproven. |
| Draw order | `src/tecmo_gameplay_scene_render.c:1408-1446` sorts active actors by world Y before drawing actors and ball | Deterministic native composition only. No original live OAM-order source proof was found. |
| Dunk cutaway | Strict TGDK same-pack CHR/palette/background/sprite data and bounded stage schedule; renderer hides court and returns to live at `scene_draw_dunk_presentation` | Native-faithful/capture-bounded. Trigger selection, live arc, roster policy, and post-return formation parity remain approximate/incomplete. |
| Close shots | Bank05 `$8ABD`, `$8ACE-$8C56`, `$8C7D`, `$8CE5-$8D7C`; TGCS numeric 0/2 exact phase tables and all bounded pose pointers; `src/tecmo_gameplay_close_shots.c:462-553` | Exact-source-pinned pose assets. Live selector, profile/direction binding, trajectory, outcome, and full caller semantics are native approximations. |
| Numeric close variant 1 | Source-backed fixed group `$10` plus direction; `scene_close_pose_for_step` at `src/tecmo_gameplay_scene_shots.c:202-237` | Pose-only native approximation. Semantic name and full object/trajectory behavior remain incomplete; the accepted public `"invalid"` name is not silently upgraded. |
| Jump shots | TGJS/TGSR selector/point/rim primitives and one bounded human away/right playback family; state matrices in `src/tecmo_gameplay_scene_test_state_flow.c` | Exact narrow primitives/playback; family gates, outcome, ordinary make arc, and broader launch ownership are native-approximate/incomplete. |
| Ball/action motion | Close shots use linear interpolation plus a synthetic parabola in `scene_update_shot_mutating`, `src/tecmo_gameplay_scene_shots.c:2340-2437` | Explicit native approximation. Exact TGCS pose cadence does not make the ball trajectory exact. |
| Pass/defense action | `scene_pass_or_switch` immediately hands off/attaches the ball; `scene_try_defense_action` applies deterministic distance/serial policy | Functional native policies, but visible pass flight/pose transition and defense/contact animation are incomplete. Foul/steal labels do not inherit raw defense evidence. |
| Violations/restarts | TGMO/TGBC detectors, TPNL metadata, TGVR strings/gesture/metasprites, and deterministic restart/camera/state tests | Exact bounded data plus native-faithful named integration. Blackout/fade, original caller order, foul detection, and full detector matrix remain incomplete. |
| Free throws | Typed lineup and two orientation checkpoints | Native integration. Aim, release, outcome, rebound, CPU placement, and full visual sequence remain approximate/incomplete. |

## Original/native boundary

The personal decompilation audit inspected the bounded Bank05 shot setup and
numeric dispatch neighborhood, its state/pose tables, and the Bank06 OAM-stream
helper contract. Those sources prove numeric routes, tables, and helper behavior;
they do not prove high-level dunk/layup selection for every caller, complete
intra-frame ordering, or live actor OAM priority.

The current C update order (rules/restarts, human action, defense, CPU,
settlement, audio, camera, frame advance) is a valid native contract, not proof
of original 6502 caller order.

## Existing proof coverage

Accepted historical evidence was report-bound during the initial audit because
the frozen worktree had no ignored build/proof directory of its own.

- Pre-tip has contiguous production frames and bounded original comparisons.
- Jump make, miss, rattle, and dunk have deterministic selected native frames.
- Shot-clock, out-of-bounds, backcourt, referee, and free-throw presentation
  have selected native checkpoints.
- Camera endpoints, fine scroll, facing, HUD, and framebuffer margins have
  deterministic contract tests.
- `gameplay-layup-frameN` now exists only for canonical frames 1 through 17 and
  the numeric-variant-2 route. No numeric-1 render mode exists.
  `gameplay-close-shot-frameN` remains explicitly a dunk compatibility alias.
- The broad scene runner's `-RequirePass` identity is pinned to an R1 lineage;
  historical R2 scene manifests therefore correctly remain `DRAFT` rather than
  claiming target-bound PASS.

## Personal visual reconciliation

Full-resolution review of the accepted native jump-make, jump-miss, rim-rattle,
and dunk contact sheets found coherent named transitions and no visible corrupt
sprite, torn frame, HUD collision, or unexplained cutaway corruption. Accepted
shot-clock, out-of-bounds, and backcourt referee frames were readable. The
left/center/right and away-facing checkpoints were clean within their narrow
fixtures.

Those results do not widen coverage. Original make/miss sheets contain denser
ten-player contexts and different camera/action composition, so the native
images do not establish frame parity. The accepted new layup sheet establishes
only the named away-left deterministic fixture; edge fixtures still do not
establish action-pose, ball, HUD, and original OAM behavior at both screen
edges.

The screenshot-QA checklist informed the implemented stable frame names and the
requirement to record scene/mode, actor geometry, camera state, HUD readability,
and full-frame hashes. No browser/FPS claim is applicable to this native desktop
port, and no performance proof was inferred.

## Broader domain audit findings

These retained gaps are outside the accepted two-path slice. Their severity
labels describe the broader presentation domain and do not contradict the
terminal auditor's all-clear P0/P1/P2/P3 disposition for candidate
`4cb0c43bcd4c7ca111c996b3788e1bd00a734424`.

- **P0:** none.
- **P1:** numeric-1, ordinary two-point, pass, and defense-action visual coverage
  is missing; the bounded away-left layup proof does not establish broad
  shot/selector/trajectory parity.
- **P1:** shot-clock reference proof is presentation-oriented; its direct
  checkpoint setup is not live detector proof.
- **P1:** original OAM ordering, full referee/foul/free-throw caller behavior,
  and broad action-at-edge behavior are unproven.
- **P2:** native state matrices substantially exceed rendered visual matrices.
- **P2:** root wording that broadly calls the referee fade "original" is wider
  than the stricter capture-bounded TGVR contract; global-doc correction is not
  currently authorized.
- **P2:** deterministic native hashes and clean selected visuals are not
  emulator-perfect parity.
- **P3:** no performance/FPS budget evidence was present in the inspected
  presentation proof.

## Smallest unanswered source questions

1. Which original caller supplies ordinary two-point and unsupported jump inputs?
2. Which original predicate selects numeric close variants across profiles,
   directions, and contact contexts?
3. Which caller/PPU sequence owns TGVR fade, foul/free-throw presentation, and
   restart timing?
4. What original ordering owns live actor/OAM priority and partial edge behavior?
5. Which original action states own pass flight and visible defense/contact
   transitions?
