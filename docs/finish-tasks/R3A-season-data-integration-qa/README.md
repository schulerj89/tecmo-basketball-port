# R3A season/data integration QA acceptance

## Signed decision

**ACCEPT** frozen candidate
`f536a19398803742e1a3dc5bb33d8a6ee2968a24` for incremental integration.
No reachable candidate defect was found.

| Classification | Open count | Integration effect |
| --- | ---: | --- |
| Candidate P0 | 0 | none |
| Candidate P1 | 0 | none |
| Candidate P2 | 0 | none |
| Inherited/internal P2 hardening residual | 1 composite item | recorded below; not a candidate blocker |
| Deferred evidence/product scope | unscored | downstream; not accepted by this gate |

This is an incremental season/data-foundation gate only. It does not accept
player-stat accumulation, populated leader results, full season progression or
save compatibility, management pages, All-Star end-to-end behavior, or complete
gameplay simulation.

## Control and Git invariants

- Task: `R3A-INTEGRATION-QA`; claim: `OWN-R3A-INTEGRATION-QA`.
- Assigned signer: `S-SOL-R3A-INTEGRATION-QA-001`.
- Master control authorization: `289a757` (`chore: authorize incremental delivery subrounds`).
- Durable master registration: `ae4b71b`; R3A was registered `combined_qa` and this task `in_progress`.
- QA worktree: exclusive control-plane allocation `<R3A_QA_WORKTREE>`; its
  private absolute path is intentionally not embedded in this committed file.
- QA branch: `codex/r3a-season-data-integration-qa-sol`.
- QA lane base, expected report parent, takeover HEAD, and last-good:
  `f536a19398803742e1a3dc5bb33d8a6ee2968a24`.
- Frozen staging ref `codex/round-3-season-data-staging` was the same exact SHA/tree.
- Product chain parent was
  `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`; both `main` and
  `origin/main` remained there throughout the product gate.
- The candidate is the exact linear seven-commit chain, in order:
  `75119657`, `3431112e`, `0c2bf410`, `d282bc21`, `15946f58`,
  `2e4ea1b5`, `f536a193`.
- `AGENTS.md`, `PORTING.md`, the complete committed R3 foundation report, and
  every commit/diff in that seven-commit history were read completely before
  QA execution.
- Takeover tracked worktree and index were clean. The candidate delta from
  `6d8f9c7a` contained exactly the eight accepted paths below and no prohibited
  tracked ROM, decompilation, capture, proof, or proprietary artifact:

  1. `docs/finish-tasks/R3-season-data-foundation/README.md`
  2. `include/tecmo_season_menu.h`
  3. `include/tecmo_team_data.h`
  4. `src/asset_pack/tecmo_asset_pack_season.c`
  5. `src/asset_pack/tecmo_asset_pack_team_data.c`
  6. `src/tecmo_season_menu.c`
  7. `src/tecmo_team_data.c`
  8. `tools/Run-SeasonTests.ps1`

The commit containing this report is required to be one direct child of
`f536a19398803742e1a3dc5bb33d8a6ee2968a24` and to change only
`docs/finish-tasks/R3A-season-data-integration-qa/README.md`. The master alone
owns any later fast-forward integration or push.

## Personal production gates

All commands were run personally by the signing Sol from the exclusive QA
worktree. In the commands below, `<LOCAL_REV1_ROM>` and
`<LOCAL_DECOMP_ROOT>` name the read-only private prerequisites supplied to the
lane; their absolute paths are intentionally normalized per `AGENTS.md`. The
private canonical ROM was read only and was not committed:

- Path token: `<LOCAL_REV1_ROM>`.
- Length: `393232` bytes.
- SHA-256:
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.

| Gate | Exact command | Exit/result |
| --- | --- | --- |
| Full repository build | `$env:TECMO_SKIP_SHORTCUT='1'; .\build.ps1` | `0`; both executables built at `/W4`, warning-clean |
| Asset-pack self-test | `.\build\tecmo_port.exe --assetpack-test` | `0`; `Asset pack self-test passed.` |
| Season self-test | `.\build\tecmo_port.exe --season-test` | `0`; `Season management self-test passed.` |
| Team Data production suite | `$rom='<LOCAL_REV1_ROM>'; $decompRoot='<LOCAL_DECOMP_ROOT>'; .\tools\Run-TeamDataTests.ps1 -ProjectRoot . -RomPath $rom -DecompRoot $decompRoot -SkipBuild` | `0`; exact `15/15`, including ROM-only TTDT parsing, All-Star mapping, transitions, malformed rejection, and 15 pixel checkpoints |
| Season production suite | `$rom='<LOCAL_REV1_ROM>'; $decompRoot='<LOCAL_DECOMP_ROOT>'; .\tools\Run-SeasonTests.ps1 -ProjectRoot . -RomPath $rom -DecompRoot $decompRoot -SkipBuild` | `0`; exact `13/13`, including strict TSNS/TSAV gates and native gameplay handoff/result |

The gameplay checkpoint proves the native menu/gameplay handoff and completed
terminal-result boundary. It injects a completed terminal result at that native
boundary; it is not evidence of a complete native basketball simulation. The
Team Data wrapper does not itself invoke the asset-pack self-test, while the
Season wrapper does; the separately run asset-pack gate above closes that test
topology without overstating either wrapper.

## Contract audit and independent recomputation

### Identity, All-Star mapping, and ranking

- Fresh `TTDT` parsed as size `96372`, FNV-1a32 `812628F0`.
- All `27 * 12 = 324` real-team player identities self-map to their canonical
  `(team, player)` keys; all 324 keys are unique.
- There are 29 unique selectors in exact order `28, 27, 0..26`.
- Canonical West source pairs are
  `12:1, 21:1, 8:2, 25:3, 23:4, 25:0, 8:0, 20:1, 20:6, 12:2, 9:4, 6:4`.
- Canonical East source pairs are
  `7:0, 3:1, 3:2, 19:3, 17:4, 4:0, 26:0, 7:1, 7:2, 1:2, 0:3, 4:4`.
- The identity contract and pack/provenance fingerprint checks fail closed on
  canonical or dependency mutation.
- The seven labels are exactly `FIELD GOALS`, `BLOCKED SHOTS`, `REBOUNDS`,
  `TOTAL POINTS`, `STEALS`, `3 POINT SHOTS`, and `FREE THROWS`.
- The caller-owned ranking seam rejects NULL inputs, invalid category/capacity,
  missing, duplicate, or invalid canonical keys, and any candidate set other
  than exactly one row for each of the 324 keys. Capacity is `1..18`; a full
  request yields 18 unique results. It orders primary descending, secondary
  descending, then later canonical scan/key for an exact tie. Candidate and
  output buffers remain transactional on failure, including a full buffer.

### Schedule and standings

- Fresh `TSNS` parsed as size `104732`, FNV-1a32 `29C64F84`; schedule offset is
  `58440`.
- Exact schedule totals for modes 82/42/26/82 are
  `1107 / 567 / 351 / 1107`; every real team appears exactly
  `82 / 42 / 26 / 82` times respectively.
- Reserved-bit, out-of-range-team, and self-match counts are all zero.
- Raw-index/ordinal mapping and end-of-schedule sentinels were audited, and
  query functions build locally before copying outputs.
- The fresh pack schedule span is byte-identical to the canonical Bank03 ROM
  span `$B52F-$BDD4`. Its FNV-1a32 is `24112737`; SHA-256 is
  `493C2AEE8C84578E70E9FB50B0EB67A882F2A0837C5B893B4227F91B4B688101`.
- Standings compare win ratios with exact 64-bit cross multiplication, then
  wins, then documented source division/order. Games behind uses the unsigned
  half-game numerator and floors a nominal negative value to zero before
  conversion, preventing negative zero. NULL, insufficient capacity, and
  invalid state fail closed without partial output.

### Save/runtime lifecycle and transactions

- Strict `TSAV-1` validates tag, version, fixed header/size, checksum, reserved
  bytes, team totals, and mode caps `82 / 42 / 26 / 82`. External TSAV contains
  aggregates, not historical result rows.
- Completed-season reset clears historical rows plus pending, blocked, and
  completion flags. Pending state must match the exact schedule record and is
  emitted once; the schedule does not advance before a result.
- Result commit requires the exact pending index/teams, valid distinct teams,
  bounded scores, and a non-tie. It persists the aggregate candidate before
  installing the historical row and advancing state.
- Team-control, season-type reset, programmed-season editor, and game-result
  failure paths restore runtime/frame/session state while retaining a useful
  failure diagnostic. Save installation uses a flushed/closed temporary file
  and replace/write-through semantics on Windows.

### Native semantic-pack boundary

- TTDT source map declares schema `tecmo.team-data/TTDT-1`, iNES-only input,
  38 bounded roles, and `chr/all` dependency. TSNS declares
  `tecmo.season/TSNS-1`, iNES-only input, 34 bounded roles, and same-pack
  `chr/all` plus team-data/menu dependencies.
- Runtime reads semantic entries from the selected asset pack, not ROM,
  decompilation, ASM, screenshots, captures, logs, or private evidence.
- TTDT, TSNS, and CHR are exact same-pack, revision, length, and fingerprint
  bound. CHR range arithmetic is overflow-safe and revalidated before use.
- Missing/malformed/oversized season entries, missing or cross-pack team data,
  missing or cross-pack CHR, and mutated payload/provenance all reject
  deterministically. The accepted pack contains no bounded prohibited source
  role token.

## Fresh ignored production-frame proof

Proof root (ignored by Git):
`build\proof\r3a-season-data-integration-qa-f536a193-019fc8d2-001`.

The proof was generated from production CLI paths with these reproducible
commands (PowerShell variables expand to the paths below):

```powershell
$rom = '<LOCAL_REV1_ROM>'
$proofRoot = Join-Path (Get-Location) 'build\proof\r3a-season-data-integration-qa-f536a193-019fc8d2-001'
$runtimeRoot = Join-Path $proofRoot 'runtime-root'
$pack = Join-Path $proofRoot 'r3a-season-data.assetpack'
.\build\tecmo_port.exe --build-assetpack $rom $pack
$env:TECMO_ASSETPACK = $pack
.\build\tecmo_port.exe --root $runtimeRoot --render-test-mode team-data-profile (Join-Path $proofRoot '01-team-data-profile.png')
.\build\tecmo_port.exe --root $runtimeRoot --render-test-mode team-data-player-detail (Join-Path $proofRoot '02-team-data-player-detail.png')
.\build\tecmo_port.exe --root $runtimeRoot --render-test-mode season-standings-east (Join-Path $proofRoot '03-season-standings-east.png')
.\build\tecmo_port.exe --root $runtimeRoot --render-test-mode season-leaders-results (Join-Path $proofRoot '04-season-leaders-results.png')
.\build\tecmo_port.exe --root $runtimeRoot --render-test-mode season-game-start (Join-Path $proofRoot '05-season-game-start.png')
Get-FileHash -Algorithm SHA256 $pack, (Join-Path $proofRoot '*.png')
Test-Path -LiteralPath (Join-Path $runtimeRoot 'saves\tecmo-season.sav')
Remove-Item Env:TECMO_ASSETPACK
```

The contact sheet is a deterministic proof-only System.Drawing composition of
the five PNGs in numeric order. It and every original frame were inspected at
full resolution by the signing Sol; the independent Luna also recomputed every
hash and inspected the same proof read only.

| Artifact | Dimensions/length | SHA-256 | Personal observation |
| --- | ---: | --- | --- |
| `r3a-season-data.assetpack` | 1,397,729 bytes | `8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141` | fresh canonical pack; production runtime source |
| `00-r3a-contact-sheet.png` | 1340x1640 | `BFCB83DDAEEB94322A20341CF566DC5C995F7C4B3CFE678A35D4D02A9E34C779` | all five states visible in numbered order |
| `01-team-data-profile.png` | 640x480 | `E8BA35AC6C2FF05F882CC6D374BC3D4578992A1304D7018A2FE2D21F25F8D575` | Atlanta identity, logo, coach, division, roster, and cursor are aligned, legible, and unclipped |
| `02-team-data-player-detail.png` | 640x480 | `BC717CC2C62A1BAD485BA6307F8F250198476AFBD816162D6311D4A960635174` | portrait/identity/attributes and six bars align; explicit `.000`/zero totals do not fabricate accumulation |
| `03-season-standings-east.png` | 640x480 | `3CC04A3C668C9EA7265D7758AA08CADB33BA5E416C1D717B37B9D595050229AB` | Atlantic/Central zero state and source order; W/L/PCT/GB are legible |
| `04-season-leaders-results.png` | 640x480 | `540D6EA78E8CB646E1D4D960E97EE5A464D04ABD32A6634BCCBD6E75F8CE7764` | all seven categories and explicit `PLAYER RESULTS UNAVAILABLE`; no fabricated leaders |
| `05-season-game-start.png` | 640x480 | `66458313C7243A8EB3C464495B5B31D1EEAD31BA5D5B8669AD1F9009B0D65649` | stable prelaunch boundary; console reports `phase=game-start-prelaunch game-pending=1 launch-blocked=1 save=0` |

The isolated `runtime-root` was empty after proof. The pre-result query returned
`PRE_RESULT_SAVE_EXISTS=False`, confirming no save was installed before a game
result. Audio is **N/A** for this slice.

## Original-evidence classification

- **Bank02 identity:** C-0176/C-0177 are classified
  `data_text_tables_mixed`, medium confidence. The 27 real-team pointer tables,
  12 players per team, and 29 profiles combine with exact canonical pointer and
  fingerprint matching for high confidence in this bounded identity mapping.
  This is not evidence for broader season/player-stat semantics.
- **Bank03 schedule:** C-0173 is `code_data_mixed`, medium confidence; C-0174
  is `data_tables_mixed`, medium/coarse confidence. Exact canonical ROM-span
  equality and exhaustive filtered counts support only the bounded schedule
  data/count contract, not undocumented scheduling policy.
- **Bank00 leader mechanics:** C-0201 provides high-confidence text/pointer
  evidence for the seven labels. C-0200 is `code_data_mixed`, medium confidence;
  its audited loops support the 27x12 scan, 18 results, primary/secondary
  ordering, and later-scan exact-tie replacement.

The evidence does **not** prove persistent accumulator lifecycle, field widths
or storage, exact category-byte meanings, original tie or result-commit policy,
original SRAM/save schema/checksum/slot compatibility, or full leader-data
population.

## Findings and bounded residual risk

### Candidate findings

None. Open candidate counts are exactly **P0=0, P1=0, P2=0**.

### Inherited/internal P2 hardening opportunity (non-blocking)

One composite inherited item exists in
`src/tecmo_team_data.c:971-984`,
`tecmo_team_data_asset_chr_available()`. It predates the seven-commit candidate
chain:

- A directly mutated, already-loaded asset can give a cursor an invalid
  `top_chr_offset` while keeping `bottom_chr_offset` valid; this revalidator
  checks only the bottom offset. The draw helper range-checks, so the observed
  effect is an absent top cursor rather than an out-of-bounds render.
- A directly mutated real-team `logo_count` above
  `TECMO_TEAM_DATA_LOGO_CELL_LIMIT` (60) can make this revalidator iterate past
  the fixed logo array because it does not first re-bound the count.

The canonical parser validates both cursor offsets and bounds `logo_count`.
Same-pack provenance, length, and hashes reject external pack mutations.
Therefore neither trigger is reachable through the accepted asset-pack/runtime
path; both require direct in-memory corruption. This is one inherited defensive
hardening P2, not a candidate rejection or accepted external vulnerability.

### Installed-result invariant classification

`src/tecmo_season_menu.c:2368-2374`, `valid_runtime_state()`, bounds-checks
historical game-result teams and scores but does not independently re-check
`away != home`, non-tie, or exact schedule identity. This is an acceptable
internal-invariant boundary and is not counted as a candidate finding.
`tecmo_season_commit_game_result()` is the only installer and requires the exact
pending schedule index/teams, valid distinct teams, bounded non-tied scores,
and successful aggregate persistence before installing the row. TSAV exposes no
historical-row setter. A malformed installed row therefore requires direct
in-memory corruption, not a reachable product path.

## Explicit downstream/deferred boundaries

This acceptance does not claim completion of:

- player-stat accumulation or population;
- populated/available real leader results UI;
- accumulator lifecycle, width, storage, or exact category-byte meanings;
- complete progression and save schema, original SRAM compatibility,
  checksum, or slot behavior;
- original tie policy or original result-commit policy;
- team-management UI/data pages;
- All-Star end-to-end behavior beyond the fixed source mapping;
- gameplay simulation beyond the native handoff/result boundary; or
- any audio behavior or proof.

These items remain downstream rather than being converted into false failures
or false completion claims in R3A.

## Independent Luna lineage and closure

- Thread: `019fc8d7-50f8-77f1-b379-8bcc33280ae5`.
- Exact title: `Tecmo R3A Season Data Integration — Independent QA Luna Max`.
- Model/thinking: `gpt-5.6-luna`, `max`.
- Allocation: projectless, read-only, branch/worktree/base/last-good all null.
- Created: `2026-08-03T18:16:22Z`; pinned immediately.
- Completed: `2026-08-03T18:34:35Z`; final app state completed/idle.
- Fault lineage: none; no bad-request, retry, replacement, or second Luna.
- Final independent decision: **ACCEPT**, candidate P0/P1/P2 all zero; the one
  inherited/internal P2 above is non-blocking.

The same Luna was reused for the initial audit and final proof review. It was
held pinned through creation of this durable acceptance record and is unpinned
only after the report commit, with that metadata reported to the master.

## Recovery and integration handoff

- Product last-good remains exact candidate
  `f536a19398803742e1a3dc5bb33d8a6ee2968a24`.
- If this report commit or its one-path invariant fails verification, do not
  integrate it. Return to the unchanged candidate SHA and regenerate/review the
  docs-only record; do not alter product files to repair documentation.
- The ignored proof directory is reproducible from the commands above and may
  be discarded without affecting the candidate. It contains no tracked asset.
- If a future reachable product trigger is found for either internal hardening
  note, route a new bounded product task to the responsible domain; do not patch
  it in this QA lane.
- The master must independently verify the report commit parent/path set and
  unchanged `main`/`origin/main` before any non-force, fast-forward integration.
  This Sol does not merge, rebase, or push.

## Attestation

- Role: `S-SOL-R3A-INTEGRATION-QA-001`.
- Sol task: `019fc8d2-986a-7643-978f-099070876dfb`.
- Master task: `019fc5d4-f360-78b3-b2a6-c8bae92df690`.
- Model/thinking: `gpt-5.6-sol`, `max`.
- Signed at: `2026-08-03T18:36:09Z`.
- Candidate: `f536a19398803742e1a3dc5bb33d8a6ee2968a24`.
- Decision: **ACCEPT for incremental integration; stop before main merge/push**.
