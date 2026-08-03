# Tests

All commands below were run from
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna`.

## Canonical input

Focused runners accepted only this test-only input:

`C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`

SHA-256:
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`

The ROM was not copied into the worktree, linked at runtime, or committed.

## Final results

| Command | Result |
|---|---|
| `.\build.ps1` | Exit 0; warning-free MSVC build produced `build\tecmo_port.exe` and `build\tecmo_port_game.exe`. |
| `.\build\tecmo_port.exe --assetpack-test` | Exit 0; `Asset pack self-test passed.` |
| `.\build\tecmo_port.exe --gameplay-state-test` | Exit 0; `GAMEPLAY STATE SELF TEST PASS replay=7A204A525C79D21C`. |
| `.\tools\Run-GameplayFatigueTests.ps1 -ProjectRoot C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna -RomPath C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes` | Exit 0; `TGFT-1 fatigue tests passed.` |
| `.\tools\Run-GameplayFreeThrowLineupTests.ps1 -ProjectRoot C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna -RomPath C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes` | Exit 0; `TGFL-1 focused tests passed` with 12 source mutations. |

The TGFT runner covers canonical 512-byte payload/fingerprint, source-map
spans, malformed dependency/entry sizes, active/bench/recovery vectors,
cadence modes, `0 -> 255`, state/input/assets/storage aliases, and transactional
replacement. The TGFL runner covers canonical 1216-byte payload/fingerprint,
strict source map, 12 selected source mutations plus the listed
dependency/size/strict-object cases, both orientations, all base placements,
and the self-test’s 720 policy
vectors plus `0xFF` predicate check.

## Review corrections represented in the final run

An earlier TGFL focused attempt exposed that the builder’s newly required
internal full-ROM SHA gate changed mutated-ROM diagnostics from source-span
messages to the full-ROM SHA message. The owned runner was updated to assert
that stronger contract; the final run passed. This was a test-contract
correction, not a product failure.

No production scene integration, Win32 visual capture, or audio proof was run
by this worker. Those are Sol-owned; visuals/audio are N/A for this API/state
lane.
