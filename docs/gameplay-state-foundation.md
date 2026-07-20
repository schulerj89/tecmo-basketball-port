# Native gameplay state and scene provenance

`tecmo_gameplay_state` remains a deterministic pure-C rules boundary, and is
now driven by `tecmo_gameplay_scene` in normal preseason and season games. The
scene renders a strict ROM-derived court and actor poses, drives native music,
SFX, and the held-ball/dribble DMC event, and returns a completed result to the
launching mode. It never loads a decompilation file, trace, capture, screenshot,
video, save state, or dump at runtime.

## Supported runtime boundary

Preseason final team confirmation now launches the selected teams, difficulty,
control ownership, period length, speed setting, and GAME MUSIC setting. A
completed preseason game returns to the stable PRESEASON row on the blue menu.
Season GAME START launches the exact pending schedule ordinal and teams. Its
non-tied result is committed exactly once through
`tecmo_season_commit_game_result` before control returns to the season result
rows; failure to commit leaves the gameplay result active rather than advancing
the schedule.

Live controls use the native NES responsibilities: directions move the owned
actor, NES A passes on offense or switches to the nearest defender, and NES B
starts an offensive shot or attempts a defensive steal/contact action. Offense
resolves before defense when both sides press B in one update. START and SELECT
are inert during live play, and a controller with no assigned team cannot act.

During a human free throw, the scene resolves the controller assigned to the
scoring team and launches only while that pad's current NES B level is held.
The other pad, A, directions, START, SELECT, B release, and a synthetic pressed
edge without the held level are ignored. A human attempt has no timeout. If the
scoring team has no assigned controller, native play uses the bounded slot-3
trace's 125-update inclusive CPU state-18-to-launch schedule (frames 22 through
146). This is an observed native approximation, not a decoded ROM timer. Its
counter resets for each attempt and across scene launch/end. Foul-card dismissal
remains the separate NES A-release presentation gate.

The compound scene loads `gameplay/core` TGPL-1 (23416 bytes,
`2047CCE0`), `gameplay/court` TGCT-1 (6559 bytes, `ECAB7A93`),
`gameplay/close-shots` TGCS-1 (3144 bytes, `DACDC976`),
`gameplay/dunk-cutaway` TGDK-1 (20272 bytes, `E02F2D21`),
`gameplay/jump-shots` TGJS-1 (1648 bytes, `7587B099`),
`gameplay/shot-resolution` TGSR-1 (384 bytes, `8486DB33`), `audio/music`
TMUS-1 (36784 bytes, `05C00ECB`), `audio/gameplay-sfx` TSFX-1 (2824
bytes, `968A5DE6`), `audio/gameplay-dmc` TDMC-1 (2515 bytes,
`AD70E6E8`), and the exact 262144-byte `chr/all` revision from one asset pack.
Exact sizes, payload fingerprints, deep indexes, reserved bytes, source-map
spans, CHR fingerprints, and the shared pack path are validated before the
scene becomes available. Missing, malformed, oversized, wrong-revision, or
cross-pack dependencies fail closed without a partial frame.

TGSR-1 also has FNV1a64 `3DDF28659B192273` and requires exact same-pack
TGPL-1. Its revision-fingerprinted sources are Bank05 `$91BC-$943A`,
`$A6EE-$A9D9`, `$B73E-$B87B`, and `$B87C-$B8F5`. The safe native semantics
are terminal outcome polarity, numeric rim-route selection, claimant
thresholds, and claimant-driven handler/possession decisions. It does not name
a rebound, block, steal, or generic make. Missing, malformed, wrong-sized,
wrong-revision, or cross-pack TGSR data rejects the scene before availability.

`gameplay/penalties` TPNL-1 is a separate strict 768-byte pure rules asset
(FNV1a32 `980DDC76`) with same-pack TGPL-1 and TSFX-1 dependencies. It exposes
bounded foul classification, violation, and presentation data without
inferring contact, collision, possession, or route state. The scene's current
deterministic contact/foul branches are implementation-owned and do not yet
consume TPNL.

The two supported close-shot families retain their numeric ROM identities.
Variant 0 is the dunk family and has 32 exact steps in the
direct/held-release family; variant 2 is the layup family and has 16 exact
steps in the arc/longer-trajectory/contactable family. Their phase
tables and all 208 TGCS-stored profile/direction resolutions into TGPL pose data
are exact assets. Live play currently selects only profile 0/direction 0 and
mirrors actor-facing-left at render time; that narrower selection and mirroring
are native approximations, not properties proved by the asset. Numeric variant
1 remains unexposed. TGCS APIs and fields continue to expose numeric IDs; the
loader derives and validates the exact 0=dunk, 2=layup semantic mapping without
changing the 3144-byte payload or its `DACDC976` fingerprint.

The dunk family now crosses into the strict TGDK-1 presentation. The importer
decodes screen `$0B` into both bounded 960-cell backgrounds, resolves the exact
profile-1/uniform-`$30` palette checkpoint, retains both side-specific seven-stage
8x16 sprite streams, and maps every background/sprite tile into same-pack
`chr/all`. Record order remains NES OAM priority, so the native renderer
composites in reverse. The observed action schedule is live 1-22, dispatch 23,
initial black 24-27, visible cutaway 28-62, black/rebuild 63-70, live return 71,
A9C5 at 87, and settlement at 132. Stage 0 is assigned at 27 and first visible at
28. Later assignment and captured first-visible frames are
32/37/42/47/52/57. Frame 63 remains black despite retained final-stage OAM; frame
64 clears OAM. The scene freezes the TGCS live pose at step 22 during the
presentation, resumes at step 23 on frame 71, reaches step 31 on frame 79, and
holds it through settlement.

Ordinary-jump support is narrower and fails closed outside its evidence. The
scene consumes the exact TGJS family-0/profile-0/direction-1 pose index 213 for
the captured human away/right terminal-miss slot. It preserves current-level NES B hold
and release, actor `$0C->$0D->$0E->0` progression, Bank05 unsigned Q8.8
height/velocity seed `$02E8`, gravity, frame-40 integer-height floor clamp,
recovery to idle pose 469 at frame 46,
ball route `$01->$05->$17->$10->0`, and settlement at frame 87. Actor and ball
lifetimes are independent. The release does not request DMC; only the proven
route-10 ground/bounce condition requests `$B5AB` at frame 75. The ball's
screen-space interpolation remains native geometry and is not claimed as the
ROM launch solver.

The ordinary-jump gate still uses an explicitly native deterministic policy:
its predicted-make branch is unsupported, while the one evidence-backed
predicted-miss branch launches. Frame 1 stores UNKNOWN; current-B release at
frame 2 passes TGJS's bit-7-set outcome flag through TGSR and requires MISS.
At frame 87 the normal route requires TGSR's non-current, OTHER_TEAM claimant
decision, awards zero points, queues crowd 11 followed by clock-gated side
result 12/13, and gives possession to an explicitly approximate opposing
actor. A simultaneous period expiry queues only crowd 11 and retains the
current side. Outcome state clears after settlement and no rebound/block/steal
stat event is synthesized.

Bounded local original execution supplies the semantic correlation. Save-state
slot 2 held numeric variant 0 through the live approach, entered its visible
cutaway, and later triggered Bank05 `$A9C5` DMC at action frame 87. A9C5 remains
address-bound and unresolved; this observation proves neither meaning nor
exclusivity.
Slot 1 held numeric variant 2 and triggered `$ABF5` at action frame 34. These
local save states, FCEUX/Lua traces, screenshots, and logs remain ignored
verification evidence only; they are not committed source-map provenance,
asset-pack inputs, or runtime dependencies.

State timing is evidence-derived: game-clock divider 45, shot-clock reset 24
with possession divider 50, an inclusive 31-update fixed expiry wait, 60-frame
period banners, a 120-frame halftime banner, four presentation lead-in frames,
120 violation wait frames, and 160 foul wait frames. Violation/foul overlays can
be dismissed only by NES A release after the lead-in; halftime and final score
screens use their separate unbounded NES A-release gate. Individual foul-out is
six, the team-bonus threshold is five in regulation and four in overtime, and
team fouls clear after the completed period/halftime banner path.

Gameplay track 5 is queued at launch and qualifying restarts only when GAME
MUSIC is enabled. Presentation track 6 is requested for halftime/final score
presentation. The scene maps clock expiry to SFX 3, late-clock seconds to 14,
violations to 6, and motion with a held ball to the proven `$B5AB`
held-ball/dribble DMC clip. A made dunk, the supported slot-0 jump miss, and
every resolved free throw, make or miss, request crowd response 11 followed by
away-side 12 or home-side 13 when the clock is above 0:01. The jump miss awards
no points. The same mailbox is last-write-wins, so the side result is consumed
next; 0:00 and 0:01 retain 11. Made layups
continue to request only 11 pending separate caller-path integration. Neutral
SFX 5 is kept as `BANK05_9FEC_CUE` and is requested only at the evidence-bounded
violation, direct-foul, and period restart boundaries under the GAME MUSIC
gate. Foul/violation and completed-period presentation boundaries clear music,
tonal SFX, and DMC once before replacement audio. Their qualifying live returns
requeue the gated cue and gameplay track 5. A foul route entering free throws
instead queues track 5 at sequence setup without the same-numbered SFX cue. The
ignored bounded slot-3 observation begins setup at frame 10, requests gameplay
track 5 at 26 and consumes it at 27, then changes the terminal result mailbox
from `$0B` to `$0D` at 280 and consumes it at 281. It is live by 300 with no new
music-track request or SFX ID 5 through 360. A final free throw therefore keeps
its result request through the live transition and queues neither track 5 nor
`BANK05_9FEC_CUE`. Dunk action frame 87 requests address-bound A9C5. Clip IDs
0, 1, and 2 remain unresolved; ABF5 and A8D6 are not queued, and no clip name
asserts an impact or rim cue.

The supported jump-miss settlement uses that same central crowd/side-result
helper independently of point accounting. Its release requests no DMC; only
the route-10 ground/bounce condition queues B5AB at action frame 75.

## ROM-derived anchors

The behavior encoded here was bounded against the Rev 1 ROM and the matching
decompilation at these CPU-address ranges:

- Bank 03 `$8374-$8378`: selectable regulation-period minute values.
- Fixed bank `$E80F-$E81E`: inclusive 31-update expiry wait.
- Fixed bank `$E58D-$E617`: period/halftime/final/overtime decisions;
  `$E59B->$E823` first prepares regulation M:00/divider 45, and only the tied
  overtime restart at `$E601-$E60F` overwrites the duration with OT minutes.
- Fixed bank `$E6ED/$E6FF`: the two team-foul clears after a completed banner.
- Fixed bank `$E765-$E76F`: shot-clock-24 and divider-50 reset.
- Fixed bank `$E7D0-$E822`: live-action settlement gate at period expiry.
- Fixed bank `$E823-$E898`: new-period M:00/divider-45 preparation and live
  clock tick behavior.
- Fixed bank `$E95E-$EA11`: foul presentation and its 160 one-frame waits.
- Fixed bank `$EA14-$EA2F`, with input helper `$D2B9-$D2CE`: four-frame
  presentation lead-in followed by dismissal on NES A release from either
  controller. Held A, directions, START, and other releases do not dismiss.
- Fixed bank `$EC5B-$ED14` and Bank 03 `$BE87-$BFA8`: violation dispatch and
  the seven numeric violation values; only shot-clock expiry is generated by
  this module.
- Bank 06 `$A05A-$A24F`: period/halftime banner selection and presentation.
- Bank 06 `$BC3C-$BCF9`: halftime/final score screen's own unbounded NES A
  release loop. It does not share the fixed-bank presentation timeout.
- Bank 05 `$94F9-$9674` and Bank 02 `$B0F8-$B398`: bounded individual/team
  foul counters, bonus-related paths, overlay state, and the two observed
  post-foul shot-24 clock-divider outcomes (45 and 50).
- Bank 05 `$8A33-$8ABD`: the human state-20 path selects the shooting side's
  controller and tests its current NES B level before launching a shot; it does
  not consume direction or button-edge state at that gate.
- Bank 05 `$96B6-$9708`: CPU ownership bypass and selection of command/script
  offsets `$007D` or `$00D7`. These values are offsets, not frame timers.
- Bank 06 `$8B8E-$8B9D`: maps the selected command offset from base `$9F2E` to
  its stream/dispatch pointer.
- Bank 06 `$9621`, `$976F-$985C`: free-throw setup/lineup state boundary. The
  visual lineup/repositioning remains outside the current scene renderer.
- Bank 05 `$8ABD-$8CE4`, table `$8CE5-$8D7C`, launch `$9C40-$9CC9`, actor
  progression `$86BB`, `$86DD`, `$8732`, `$8745`, result `$91BC-$943A`, ball
  path `$AF30-$B073`, and scoring `$B995-$BA3F`: numeric close-shot subtype 01
  and its surrounding shot machinery.
- Bank 05 `$AD01-$AD0E` (FNV1a32 `B7141C72`): result crowd-response request 11;
  `$8C7D-$8CE4` (FNV1a32 `00A4D185`) is its bounded close-shot caller path.
- Bank 05 `$B1D1-$B1E6` (FNV1a32 `CFCD9759`): above-0:01 clock gate and
  pre-handoff side-result request 12/13; `$B19D-$B1A4` (FNV1a32 `ED5EE105`)
  is the bounded result caller path. `$BA65-$BA9C` (FNV1a32 `35FB80C4`) and
  `$B87C-$B888` (FNV1a32 `E903D8F9`) supply the integrated jump-shot settlement
  caller evidence.
- TGSR-1 revision-locks Bank 05 `$91BC-$943A` (`4A0C68AC`),
  `$A6EE-$A9D9` (`21A416FD`), `$B73E-$B87B` (`574FEE44`), and
  `$B87C-$B8F5` (`9E2F1F28`) for terminal polarity, numeric rim dispatch,
  claimant scanning, and claimant-driven settlement respectively.
- TPNL-1 revision-locks Bank 05 `$9571-$9649`, Bank 02 `$B0F8-$B398`,
  fixed `$E95E-$EA11`, `$EA14-$EA2F`, `$EC5B-$ED14`, and `$D2B9-$D2CE`,
  Bank 03 `$BE87-$BFA8`, and Bank 04 `$BA1F-$BA3E`. These feed the pure
  penalty asset/API only; they are not a claim that the live synthetic contact
  branches use ROM classification.
- Fixed `$EC06-$EC25` (FNV1a32 `F1BCC8E2`): clears active music, SFX, and DMC;
  bounded call sites are `$E58D`, `$E9A0`, `$E9DE`, and `$ECAF`.
- Bank 05 `$856B-$85A7` and `$85F3-$8640`: variant-0 presentation trigger and
  clear-lane helper.
- Fixed `$E770-$E78D` and descriptor `$DCD2-$DCD8`: presentation dispatch and
  screen `$0B` selection; Bank 00 `$9022-$9346` plus `$9346-$9355` supply the
  overlapping D9F6 terminator/base-palette boundary.
- Bank 01 `$B002-$B157`: controller, seven-stage setup/tables, and palette
  recipe; fixed `$C711-$C73B` plus `$CAF5-$CBAE` supply selector dispatch.
- Bank 06 `$B37C-$BC3B`: relative sprite emitter, side pointers, and all
  four-byte geometry records; fixed `$EB8D-$EC05` restores the court.
- TGJS-1 owns the otherwise-unpacked Bank05 spans `$8469-$846A`,
  `$8999-$89C0`, `$8D92-$8DD2`, `$9C29-$9C3F`, `$AD41-$AF21`,
  `$B6E5-$B774`, `$B7C1-$B87B`, and `$BA65-$BAC0`. It depends on TGPL-1 for
  actor dispatch/poses/results and TGCS-1 for dispatcher, launch-solver, Q8.8,
  and route tables already covered there. The 32 normalized pose indices are
  rederived from Bank05 `$8D3D/$8D5D` and have FNV-1a `A057A625`.

The corresponding lifted sources include
`decomp/lifted/bank03/C-0144_bank03_selection_value_table_8374_8378.asm`,
`decomp/lifted/bank05/C-0095_bank05_state_and_pose_lookup_tables_8CE5_8D7C.asm`,
`decomp/lifted/C-0005_bank05_91BC_943A.asm`,
`decomp/lifted/bank05/C-0111_bank05_large_state_and_trajectory_cluster_985B_BFA7.asm`,
`decomp/lifted/bank06/C-0055_bank06_period_banner_dispatch_A05A_A0A9.asm`,
and
`decomp/lifted/bank02/C-0177_bank02_roster_team_player_data_9000_BFFF.asm`.
These are provenance only and are not runtime inputs.

## Explicit evidence boundaries

- Only an allowed live action reported on the update that reaches zero enters
  unbounded settlement; earlier action history is ignored. A later settled
  report completes that sequence, while an initially settled zero-clock state
  follows the fixed 31-update path.
- Every completed period first prepares regulation M:00/divider 45 through
  `$E823`, then chooses its banner, halftime, overtime, or final-score branch.
  Only a tied overtime restart at `$E601-$E60F` overwrites that duration with
  OT minutes; a completed overtime final retains regulation minutes.
- Foul subtype/detection, which counters a foul changes, post-presentation
  possession, and selection of divider 45 versus 50 are caller-supplied.
  Unsupported or malformed choices fail without mutating state.
- Free-throw controller ownership and the human current-B launch gate are
  supported. CPU play uses the bounded observed 125-update launch schedule;
  original positioning/script dispatch, visual lineup timing, aiming,
  made/missed policy, rebound behavior, and post-attempt possession remain
  unresolved. Only explicit made/missed results and settlement are modeled.
- The exact TGCS numeric step/phase tables and the selected TGPL pose resolution
  are consumed directly by the scene. TGCS exposes 208 exact resolutions, but
  live selection is limited to profile 0/direction 0 and actor-facing-left is
  mirrored during rendering. Those live policies remain approximations. The
  older state-only rightward actor-9 observation remains provenance for the
  semantic event layer, not a universal animation label.
- TGJS/TGSR live playback proves only the human away/right
  family-0/profile-0/direction-1 terminal-miss slot. Its current-B transition,
  actor Q8.8/state/recovery timing, ball route-state checkpoints, conditional
  bounce DMC, MISS polarity, and one post-shot claimant settlement are exact
  within that context. Unsupported profiles/directions/outcomes, generic
  makes, the longer +157-update claimant route, and unknown horizontal launch
  geometry do not inherit those frame checkpoints. No semantic rebound, block,
  steal, or player-stat event is claimed.
- Actor starting layout, camera/orientation composition, movement and AI,
  jump-ball screen interpolation, unsupported jump routes, general
  make/contact policy, the distance policy
  selecting dunk/variant 0 versus layup/variant 2, live close-shot
  profile/direction selection and left-facing render mirroring, dynamic
  team/court palette selection, foul detection, free-throw
  lineup/aim/result/rebound and CPU positioning/script behavior, and
  HUD typography are native approximations. The imported TGCT palette bytes and
  embedded FCEUX RGB profile are exact, but native selection does not yet
  reproduce all original matchup/state colors. The exact rules state consumes
  explicit outcomes without turning those scene policies into ROM-exact claims.
- The dunk cutaway uses the exact bounded profile-1/uniform-`$30` checkpoint;
  selecting a different team profile/uniform remains unresolved. Its native
  shot arc and deterministic make/miss policy continue behind/after the exact
  presentation and are not claimed as ROM behavior.
- Local original-frame comparisons, after normalizing the small FCEUX screenshot
  RGB-output difference, matched the frame-24 black, frame-32 stage, frame-48
  stage, and frame-64 black cutaway pixels exactly. Returned live frame 80 still
  differs because camera, spacing, HUD, and dynamic matchup palette selection
  remain native approximations.
- The module contains no proprietary ROM bytes, screenshots, traces, save
  states, dumps, or capture artifacts.

Run `tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for the
strict full-pack scene test and deterministic 640x480 start, ordinary-jump, and
dunk checkpoints through frame 132. Run
`tools\Run-GameplayDunkCutawayTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for
the strict TGDK payload/provenance/render/mutation/revision checks.
`Run-GameplayShotResolutionTests.ps1` and `Run-GameplayPenaltyTests.ps1`
validate the strict TGSR/TPNL parsers, same-pack dependencies, source mutation,
and pure APIs. `--gameplay-state-test`, the TGPL/TGCT/TGCS/TGJS focused suites,
the 74-entry full asset-pack regression, and `Run-GameplayAudioTests.ps1`
retain their lower-level coverage.
