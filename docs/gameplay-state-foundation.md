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

The compound scene loads `gameplay/core` TGPL-1 (23416 bytes,
`2047CCE0`), `gameplay/court` TGCT-1 (6559 bytes, `ECAB7A93`),
`gameplay/close-shots` TGCS-1 (3144 bytes, `DACDC976`), `audio/music`
TMUS-1 (36784 bytes, `05C00ECB`), `audio/gameplay-sfx` TSFX-1 (2824
bytes, `968A5DE6`), `audio/gameplay-dmc` TDMC-1 (2515 bytes,
`AD70E6E8`), and the exact 262144-byte `chr/all` revision from one asset pack.
Exact sizes, payload fingerprints, deep indexes, reserved bytes, source-map
spans, CHR fingerprints, and the shared pack path are validated before the
scene becomes available. Missing, malformed, oversized, wrong-revision, or
cross-pack dependencies fail closed without a partial frame.

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

Bounded local original execution supplies the semantic correlation. Save-state
slot 2 held numeric variant 0 through the live approach, entered its visible
cutaway, and later triggered the Bank05 `$A9C5` DMC sequence at action frame 87.
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
violations to 6, made shots/free throws to crowd response 11, and motion with a
held ball to the proven `$B5AB` held-ball/dribble DMC clip. Neutral SFX 5 is
kept as `BANK05_9FEC_CUE` and is requested only at the evidence-bounded
foul/restart boundaries under the GAME MUSIC gate. Imported side-result SFX
12/13, the conservative dunk-sequence A9C5 and layup-sequence ABF5 clips, and
the two still-address-bound A8D6 clips are not queued by the live scene. The
sequence names do not assert an impact or rim cue.

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
- Bank 05 `$8ABD-$8CE4`, table `$8CE5-$8D7C`, launch `$9C40-$9CC9`, actor
  progression `$86BB`, `$86DD`, `$8732`, `$8745`, result `$91BC-$943A`, ball
  path `$AF30-$B073`, and scoring `$B995-$BA3F`: numeric close-shot subtype 01
  and its surrounding shot machinery.

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
- Free-throw aiming, release timing, rebound behavior, and post-attempt
  possession are unresolved. Only explicit made/missed results and explicit
  settlement are modeled.
- The exact TGCS numeric step/phase tables and the selected TGPL pose resolution
  are consumed directly by the scene. TGCS exposes 208 exact resolutions, but
  live selection is limited to profile 0/direction 0 and actor-facing-left is
  mirrored during rendering. Those live policies remain approximations. The
  older state-only rightward actor-9 observation remains provenance for the
  semantic event layer, not a universal animation label.
- Actor starting layout, camera/orientation composition, movement and AI,
  ordinary jump-shot timing, shot arc, make/contact policy, the distance policy
  selecting dunk/variant 0 versus layup/variant 2, live close-shot
  profile/direction selection and left-facing render mirroring, dynamic
  team/court palette selection, foul detection, free-throw
  aim/timing/result/rebound behavior, and
  HUD typography are native approximations. The imported TGCT palette bytes and
  embedded FCEUX RGB profile are exact, but native selection does not yet
  reproduce all original matchup/state colors. The exact rules state consumes
  explicit outcomes without turning those scene policies into ROM-exact claims.
- Local original-frame comparisons at gameplay start and ordinary-jump/dunk
  checkpoints found no unrendered or garbage cells and kept exact assets
  and poses stable. Camera, spacing, HUD, and dynamic matchup palette selection
  remain visibly non-identical.
- The module contains no proprietary ROM bytes, screenshots, traces, save
  states, dumps, or capture artifacts.

Run `tools\Run-GameplaySceneTests.ps1 -Build -RomPath <LOCAL_ROM.nes>` for the
strict full-pack scene test and deterministic 640x480 start, ordinary-jump, and
dunk checkpoints. `--gameplay-state-test`, the TGPL/TGCT/TGCS focused
suites, and `Run-GameplayAudioTests.ps1` retain their lower-level coverage.
