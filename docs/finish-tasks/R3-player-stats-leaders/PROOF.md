# Proof record

All commands below were run from
`C:\Users\joshs\Projects\tecmo-basketball-port-r3-player-stats-leaders-luna`.
The executable checks were run after a warning-free `build.ps1` build; this
record describes the resulting proof-capture state.

## Executed commands and results

| Command | Result |
| --- | --- |
| `.\build.ps1` | Built `build\tecmo_port.exe` and `build\tecmo_port_game.exe`; no compiler warnings/errors. |
| `.\build\tecmo_port.exe --season-test` | `Season management self-test passed.` |
| `.\build\tecmo_port.exe --gameplay-scene-test .\build\live-proof-20260804T112244013Z\asset-pack\gameplay-proof.assetpack` | `GAMEPLAY SCENE SELF TEST PASS` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-SeasonTests.ps1 -SkipBuild -AssetPackPath .\build\live-proof-20260804T112244013Z\asset-pack\gameplay-proof.assetpack` | `SEASON TEST PASS ... and 19 pixel checkpoints` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-TeamDataTests.ps1 -SkipBuild` | `TEAM DATA TEST PASS ... and 15 pixel checkpoints` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-GameplaySceneTests.ps1 -Build -RomPath "C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes"` | `GAMEPLAY SCENE TEST PASS`; LIVE proof draft emitted with 2 native videos, 14 frames, and a 1920x1440 contact sheet. |

The final gameplay proof artifacts from that run are under the ignored build
output directory:

`build\live-proof-20260804T112244013Z\PROOF-MANIFEST.json`

The deterministic scene asset pack used by the focused scene command is:

`build\live-proof-20260804T112244013Z\asset-pack\gameplay-proof.assetpack`

The `Run-GameplaySceneTests.ps1 -RequirePass` mode was not used for this R3
worker because that legacy harness hard-codes the R1 live-foundation branch and
base; its ordinary Rev1-ROM proof path passed as recorded above.

## Post-correction determinism matrix

The final personal post-correction rerun matrix recorded two consecutive PASS
runs for each focused suite: Season with all 19 checkpoints, TeamData with all
15 checkpoints, and GameplayScene using
`build\live-proof-20260804T113605505Z` followed by
`build\live-proof-20260804T113742566Z`.

Across each paired run, 255 files were present in each run, zero files were
missing, and 245 corresponding files were byte-identical.  The remaining 10
corresponding files were `PROOF-MANIFEST.json` plus its nine
negative/stale variants.  Those 10 raw manifests were not claimed to be
byte-identical: their differences were limited to run-root identifiers, proof
timestamps, and the derived manifest-inventory SHA values.  After
canonicalizing exactly those ephemeral/derived fields, all 10 corresponding
manifests were identical.

## Render checkpoint hashes

These are the exact SHA-256 framebuffer hashes asserted by
`tools/Run-SeasonTests.ps1`.  The populated entries include category 0 pages
0/6/12 and proof-only category fixtures for categories 3, 5, and 6; the
existing unavailable leader-results checkpoint remains in the table.

| Mode | SHA-256 |
| --- | --- |
| `season-team-control` | `ED752BB5D977D72AA05B136D6800AB5F7A94EB3A40EB8614D03A41F0D7557324` |
| `season-schedule` | `C237B9EF2318108D821DABABF46D2B891ACAFE7468705AA32BAE3390770C2D5E` |
| `season-schedule-popup` | `449A17BB6E83875BB7AECC2D796CCD4F2A11183B28DE6A54F21BAEFC6AC7E03A` |
| `season-playoff` | `FB074365D44606A973CFAB124FBF1870ADB2F253A3540083B429CCF003BD529A` |
| `season-playoff-mid` | `665BFBBAC1FADDE02391AB880F84D8289D291D9A021DFB80583E04D36575D502` |
| `season-playoff-east` | `1F63907C61631329250454D3317E0929D9384A7DC4E30A53680B63F4A48ED0AA` |
| `season-standings-east` | `3CC04A3C668C9EA7265D7758AA08CADB33BA5E416C1D717B37B9D595050229AB` |
| `season-standings-west` | `96C6B839321B82393D706C325610A800B8FA2B8368B662719DF8EB45DFC9387B` |
| `season-standings-programmed` | `972415E9F5C8E7AA4305E386AD156F49C8637EB4AA8AD0C7E3E104232A4E0FB9` |
| `season-leaders` | `BF506207114DC4E2823852147DFCD05E455496CE08DA502E3238A439A4877823` |
| `season-leaders4` | `1F735E60C69435EDFB2099C5A68CE630A0BC84E73E5092D8A107ED1F5BC55DEF` |
| `season-leaders-results` | `1B65D684B43BC1EB31B205CD463F4E3093E0AA19D6C7A45EB7C0162E4B5A61CA` |
| `season-leaders-results-populated` (category 0, page 0) | `600E13073B9D8509E7E5648E8AFA5221E7E038CB51D28C40AA952E5B4B80C1AB` |
| `season-leaders-results-populated-page6` (category 0, page 6) | `F07071A9032AB6CD6B2307ED4C007AE1995B5C8CD4E37A1F205D0890368AAE14` |
| `season-leaders-results-populated-page12` (category 0, page 12) | `9C2C058CA7EB355C48ED6533536088A641D7866B16EB57C5CF01410F1FEF4FD1` |
| `season-leaders-results-populated3` (category 3) | `794DA4AE2CC6FB0B75B1F30A4F682B565F5B16A3DBC26BD0E594ABC9763A182E` |
| `season-leaders-results-populated5` (category 5) | `74871EF3FFE4EA643CD707A95B29389EC487552AB9B4D7571F3B56A526EB96FE` |
| `season-leaders-results-populated6` (category 6) | `D2561DF4460C843B85127C1B6D4AA59DBDC0640DCF186562796BAF0FCB5F1FBD` |
| `season-game-start` | `66458313C7243A8EB3C464495B5B31D1EEAD31BA5D5B8669AD1F9009B0D65649` |

## Fresh native-row visual proof

After the proof-fixture correction, the six frames were regenerated under
`build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\`
and inspected at the original 640x480 resolution.  The pre-correction
attempt/make seed and its six old expected hashes are superseded.  The
intermediate `.destpal` and `.native-row` filenames were not used as
provenance baselines because they preceded the final fixed-role source/rebuild.
Nearest-neighbor raw-pixel crops resolved the apparent scaled-preview artifact:
there was no glyph fragmentation in the corrected files.  The corrected set
below is the accepted native-port evidence.

| Frame | UTC written | SHA-256 |
| --- | --- | --- |
| `build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\season-leaders-results-populated.png` | `2026-08-04T19:00:01.0479360Z` | `600E13073B9D8509E7E5648E8AFA5221E7E038CB51D28C40AA952E5B4B80C1AB` |
| `build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\season-leaders-results-populated-page6.png` | `2026-08-04T19:00:01.1734641Z` | `F07071A9032AB6CD6B2307ED4C007AE1995B5C8CD4E37A1F205D0890368AAE14` |
| `build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\season-leaders-results-populated-page12.png` | `2026-08-04T19:00:01.2235685Z` | `9C2C058CA7EB355C48ED6533536088A641D7866B16EB57C5CF01410F1FEF4FD1` |
| `build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\season-leaders-results-populated3.png` | `2026-08-04T19:00:01.2765676Z` | `794DA4AE2CC6FB0B75B1F30A4F682B565F5B16A3DBC26BD0E594ABC9763A182E` |
| `build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\season-leaders-results-populated5.png` | `2026-08-04T19:00:01.3336266Z` | `74871EF3FFE4EA643CD707A95B29389EC487552AB9B4D7571F3B56A526EB96FE` |
| `build\integration-qa-r3-20260804T184202450Z\season-leaders-frames-corrected\season-leaders-results-populated6.png` | `2026-08-04T19:00:01.3838460Z` | `D2561DF4460C843B85127C1B6D4AA59DBDC0640DCF186562796BAF0FCB5F1FBD` |

Raw-pixel QA found white title-region counts of 1,216 on category-0 pages
0/6/12, and matching complete dynamic player/team glyph regions across the
supported category frames; every final frame byte-matched its inspected
`.native-row` counterpart.  Full-resolution inspection showed the category
title, six player names, six team names, fixed orange labels, and formatted
metrics visible.  The four obsolete master baselines are explicitly rejected:
category 0 `7F259B0BF829301ACF2653F661054564E25AA3F70537FF9772AD987CED490604`,
category 3 `5C10941D261C1ED8EE8E748A6EB11EC461E30AC88FAF02217BBEBA1AF5220EAE`,
category 5 `63169AD82C7FE1CCE2C92463447F2A624267F351A8695EF9BCFC992C798D169A`,
and category 6 `CF9DD10437141EEEAD47900AC06530517768AC1EA5B177C3EA9A1ED3641EE80A`.

## Gameplay proof coverage

The authorized state-flow self-test covers both gameplay sides and all twelve
roster identities through the direct matrix, then proves the real seams:

- validated 2/3-point launches record FGA and conditional 3PA;
- real make paths record FGM and conditional 3PM once, while real misses leave
  both make counters zero and later frames do not double-count;
- away, home, and CPU free-throw paths record FTA for every result and FTM
  only for makes;
- malformed shot launch and failed settlement snapshots are unchanged;
- threshold-frame CPU free-throw failure uses a valid bound shooter with
  coverage zero and restores the full entry scene apart from status;
- 255-to-0 wrapping, result copy, exact coverage, subset/extra/zero rejection,
  and counters 6-8 staying zero are asserted;
- the synthetic one-point scoreboard timing fixture uses the normalized
  non-three path and asserts FGM consistency rather than bypassing stats.

## Season/save and malformed matrix

The season self-test commits a real two-sided ledger with every roster slot and
all six implemented counter kinds populated, checks canonical away/home
mapping, and leaves unsupported totals zero.  It proves:

- carry `250 + 8 == 258` in uint16 season totals;
- separate `65535 + 1 == 0` uint16 wrap and other uint16 wrap behavior;
- replay and conflicting result rejection preserve the full state;
- a blocked save rejects a result carrying nonzero ledger data and rolls back
  W/L, schedule, and stats together;
- the v2 saver emits exactly 5,942 bytes, preserves the 84-byte prefix, writes
  coverage and team-major/roster-minor/counter-minor totals in little endian,
  and round-trips the full totals array by `memcmp`;
- v1 schedule-zero migration yields `0x003f`, in-progress v1 migration yields
  `0x0000`, and later commits do not heal the latter;
- malformed parse rejection leaves a session snapshot unchanged.

`Run-SeasonTests.ps1` executes these file-level cases with a valid legacy
fallback present where relevant:

| Version/case | Matrix entry |
| --- | --- |
| v1 header/version/payload | `v1-bad-version`, `v1-bad-header`, `v1-bad-reserved`, `v1-bad-checksum`, `v1-truncated`, `v1-wrong-size` |
| v1/v2 physical-size dispatch | `v1-marked-v2-physical108`, `v2-marked-v1-physical5942` |
| v2 header/version/payload | `v2-bad-version`, `v2-bad-header`, `v2-bad-reserved`, `v2-bad-checksum`, `v2-unknown-coverage`, `v2-unsupported-counter`, `v2-wrong-size`, `v2-truncated`, `v2-trailing` |
| separate physical v1 trailing | `save-trailing` (109-byte v1 buffer) |
| precedence/fallback/atomic migration | `save-new-preferred`, `save-malformed`, `save-migration`, `save-v1-fresh-migration`, `save-install-failure` |

The malformed-new cases install a valid legacy file beside the malformed new
file and assert the new file is rejected without falling through to legacy.
The v2 fixture is season type 1 so its first 84 payload bytes exactly match the
v1 prefix used by the preservation assertion.

## Formula, ranking, and page proof

`tecmo_season_self_test` directly covers all seven pure projection cases while
keeping the three unsupported categories unavailable in production rendering.
Vectors include 0/0, nonzero/0 saturation, `0xffff -> magnitude 1`,
`0x8000 -> magnitude 32768`, `0x7fff * 1000 / 1 -> 0xffff` high-bit exclusion,
integer truncation, threshold boundaries, simple and wrapped composite points,
and actual unequal team-game denominators.

The task-local ranking proof seeds all 324 canonical candidates, verifies 18
outputs, checks later-key tie wins (`323..306`), and checks a metric above
`0x00ff` (`0x02ee` for 750) is compared high byte first and reconstructed for
formatting.  Paging proves `0 -> 6 -> 12`, rejects a further right move, and
returns `12 -> 6 -> 0`; a zero-first destination, unsupported category, and
incomplete coverage cannot advance.  Representative formatting is asserted
as `1.234` and `12.34`.
