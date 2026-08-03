# R1 CPU lifecycle lineage and review register

## Authority and worker

| Role | Thread/session | Branch/worktree | State |
| --- | --- | --- | --- |
| Authoritative Sol | `019fc61e-0f2a-7fb0-a76e-e4676808c959` | `codex/r1-gameplay-foundation-sol` / `C:\Users\joshs\Projects\tecmo-basketball-port-r1-gameplay-foundation-sol` | owns acceptance and final proof |
| Current pinned Luna | `019fc63f-dada-7de2-bae3-9e809126ccbe` | `codex/r1-cpu-play-lifecycle-luna` / `C:\Users\joshs\Projects\tecmo-basketball-port-r1-cpu-play-lifecycle-luna` | `gpt-5.6-luna`, max; pinned; draft/no commit |
| Trace/QA research Luna | `019fc628-0b32-7e83-b969-b41990b36e9b` | `Tecmo R1 CPU Lifecycle -- Trace and QA`; read-only research | `gpt-5.6-luna`, max; completed/accepted/unpinned; supplied route/trace hooks and selector evidence |
| Original-stream research Luna | `019fc628-0649-7c53-8153-31f8cb75c30d` | `Tecmo R1 CPU Lifecycle -- Original Stream`; read-only research | `gpt-5.6-luna`, max; completed/accepted/unpinned; supplied corpus/handler/transport evidence |

The current Luna was created at `2026-08-03T06:11:41Z` with the title
`Tecmo R1 CPU Play Lifecycle -- Native Engine Implementation -- Luna Max`.
The worker is pinned to this exact branch/worktree/base and has made no
mutation, commit, merge, rebase, push, task creation, or pin change outside
the CPU-owned boundary. Both research Lunas completed and were accepted before
being unpinned; neither mutated a worktree or committed. The lineage records
zero literal `{detail: bad request}` faults and zero confirmed equivalents.
`gpt-5.6-luna`/max, no-subagent rule, and CPU-only ownership remain active.

## Revision and review history

1. Clean expected parent was `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.
2. Research updates established the exact Rev1 ROM, corpus, route selector,
   fixed links, formation boundary, shot predicate, and dynamic-candidate
   evidence limits.
3. The first core slice added tagged contracts, native metadata/effects,
   formation/route parsing, direct-ROM anchor validation, and focused tests.
4. Sol corrections required controller-slot count 2, explicit advance policies,
   no raw shot handoff, separate fixed/dynamic link state, opcode 17/18/19
   shared barrier metadata, parse-time formation decode, and corrected `$06CB`
   comments.
5. Further Sol review removed invented execution: only opcode 1 chains;
   opcodes 7/10/11/12/13/15/16/20 were constrained or deferred; opcode 14,
   opcode 21, opcode 22, orientation, BA, wait expiry, and transactional alias
   cases received exact inputs/goldens.
6. The final core gate added opcode-10 same-offset defer, `$0005` goto-chain
   budget goldens, bad route-tag/index transaction tests, and seven additional
   ROM mutation checks. The focused runner passed warning-clean.
7. Sol recorded the corrected core control-plane checkpoint at
   `dea1fd7c2c2761fe08a6a27ab13a5e661e2b7094` and advanced
   `luna_revision -> sol_review`. It is not a product commit and is not used as
   a product proof SHA; the worker checkout remains based on the expected
   parent until Sol's integration state is reconciled.
8. This phase adds only the contract documentation and separate CPU proof
   surface. No private FCEUX proof is launched and no commit is made.
9. Master recorded the proof rejection at control commit
   `5851836` (`sol_review -> luna_revision`). The five formal findings are
   carried into this draft: the original schedule must prove setup, tip, and a
   running clock before capture; a complete window with no source-pinned CPU
   lifecycle events must fail closed; `$8BAE` is the indirect dispatcher and
   `$8BE1` is opcode 22's handler; `$8BB1/$8BC9` are static handler-table data,
   not executable hooks; and actor slot 10 has no `$06CB` fixed-link entry.
10. The renewal pass tightened the gate to require the exact stopped-clock
   tuple (`$0357=4`, `$0358=0`, `$058A=$18`) before accepting a running-clock
   transition, and added that field to Lua status, the PowerShell validator,
   and static regression checks. The current draft remains uncommitted.
11. Sol's executable-proof review rejected the next draft for twelve concrete
   contracts: `-RequirePass` must imply `-RequireVideo`; dual videos need JSON
   `nb_read_frames` and equal hashes; every log must be nonempty and inventoried
   with runner metadata; progress must be read fail-closed and finish validly;
   both original contact sheets and exact native dimensions are required; the
   live-common side/actor gate must be complete; slot 10 needs double `NA`;
   trace labels need split address/semantic confidence; the map comment and
   schedule wording must be corrected; docs must be ASCII-clean with this
   rejection recorded; `-RequirePass` must reject nonignored untracked files;
   and static tests must assert every renewal. The master durability override
   further requires explicit nonempty silent-stream log records.
12. The authorized dirty-tree rehearsal at ignored output
   `20260803-035113` was rejected before FCEUX: native build succeeded, but
   direct array-splat invocation of `Run-GameplayCpuSteeringTests.ps1` bound
   `-RomPath` positionally and failed `Resolve-Path`; parent `Stop` also let
   the child error escape before `cpu-focused.log` metadata was written. The
   output is preserved. The correction uses nested `powershell.exe -NoProfile
   -ExecutionPolicy Bypass -File` named arguments and makes `Invoke-Logged`
   catch, log, inventory, and return child invocation failures as nonzero.
13. The second authorized dirty-tree rehearsal at ignored output
   `20260803-035908` passed named wrapper/logging and produced nonempty
   focused, asset-pack, and self-test logs, but `original-1` missed the
   five-second startup sentinel. The preserved solo diagnostic
   `diagnostic-visible-startup-solo/status.txt` recorded
   `stop_reason=uncaught Lua error: attempt to yield across metamethod/C-call boundary`
   at `sequence=1`, `emu_frame=0`, with all lifecycle/capture counts zero.
   The bottom-level outer `xpcall` enclosed `FCEU.frameadvance`; it is now
   removed while the non-yielding per-frame `xpcall(execute_frame, ...)` and
   emergency finalizer remain.
14. The third authorized dirty-tree rehearsal at
   `20260803-040852` reached startup/progress (`sequence=40`, `emu_frame=38`,
   `stage=boot`) but falsely reported the 64 MiB cap. The preserved tree was
   exactly `1,405,077` bytes against a cap of `67,108,864` bytes. PowerShell
   parsed all three `Get-SessionBytes` comparisons as command arguments; all
   three are now parenthesized, and the static suite requires one `$Path` and
   two `$OutputRoot` corrected comparisons with no unparenthesized form.
15. The fourth authorized dirty-tree rehearsal at
   `20260803-041248` passed the prior wrapper, startup, and cap checks but
   rejected before proof completion with `FCEUX 'original-1' exited .` (blank
   exit text). The preserved FCEUX logs later contain runner metadata with
   `exit=0`; the preserved `progress.txt` reached
   `sequence=81`, `emu_frame=79`, `stage=finished`, and `speedmode_ok=true`,
   while `status.txt` recorded `result=abort` and
   `stop_reason=CPU lifecycle progress sentinel could not publish`.
   The process defect was an exit-code decision made before the final
   redirected-stream wait/refresh/cache path. The progress defect was a
   `ReadAllText` reader whose Windows share mode could block Lua's
   remove/rename, combined with a single-shot publish attempt. The draft now
   caches the exit code only after parameterless `WaitForExit()` and before
   log/dispose, reads progress through a bounded `FileStream` with
   `ReadWrite|Delete` sharing and deterministic disposal, and retries the
   Lua remove/rename publish three times. Named static regressions cover the
   cached post-wait process status, no post-dispose property read, the
   share-safe reader, and bounded publish retry.
16. The fifth authorized dirty-tree rehearsal at
   `20260803-042609` passed setup, tip, stopped-clock, and live gates, then
   stalled under the watchdog at `sequence=4101`, `emu_frame=4099`,
   `stage=running-clock-live`, with setup/tip/live true, `captured_frames=0`,
   and `speedmode_ok=true`. It had no status and empty trace/actor bodies.
   The clean visible diagnostic at
   `diagnostic-visible-live-callback` reproduced the marker and displayed
   GUI-only `Lua run error (null) (null)` with an empty Lua console at the
   first live-enabled exec callback; no FCEUX instance remains. The trace
   header had 30 fields but its formatter had 31 conversions and misaligned
   types: `fixed_link_text()` reached a `%02X`, address confidence reached a
   `%d`, and a final `%s` had no value. That callback was outside the
   per-frame `xpcall`. Separately, `record_hook` accepted `live_seen` with
   `capture_start_frame >= 0` while `captured` was still zero, recording
   pre-window evidence. The draft now uses exactly 30 schema-aligned
   conversions and indexed values, statically counts header/conversion/value
   positions, wraps every `registerexec` callback in a bounded non-yielding
   `xpcall` with sanitized deferred failure, and requires the defined capture
   frame plus `captured` in `1..reference_window.frames` before recording.
17. The sixth authorized dirty-tree rehearsal at
   `20260803-043857` executed cleanly and failed closed after all setup, tip,
   stopped-clock, and live gates. It recorded `setup_frame=3929`,
   `tip_start=3929`, `live_start=4101`, `capture_start=4125`,
   `max_frames=4200`, `captured=76/120`, and `screenshots=8/12`, with
   `stop_reason=maximum frame cap reached before complete live window`.
   Final progress was `sequence=4200`, `emu_frame=4198`,
   `stage=finished`, `speedmode=true`; positive evidence was already valid:
   `trace_rows=3163`, `actor_rows=836`, fetch/opcode/dispatch `327` each,
   handler `708`, advance `325`, aligned `327`, fixed-link `327`, and zero
   mismatch/invalid/misaligned counts. The inclusive 120-frame window ended
   at `4125 + 120 - 1 = 4244`, beyond the old cap, and observed trace density
   projected about `5061` rows beyond the old `4096` cap. The proof capacity is
   now explicitly `max_frames=4320` and `trace_rows=8192`, with the existing
   8 MiB text and 64 MiB session caps retained. Lua rejects an infeasible
   inclusive window at live transition. This is empirical deterministic
   schedule capacity, not ASM/source-pinned semantics; static regressions
   lock map/runner equality, reject old capacity declarations, and verify the
   inclusive runway formula.
18. The seventh authorized dirty-tree rehearsal at
   `20260803-044527` is preserved and remains rejected. The renewed
   `4320/8192` capacity worked: `original-1` completed/pass with
   `setup_frame=tip_start_frame=3929`, `live_start_frame=4101`,
   `capture_start_frame=4125`, inclusive capture through `4244`,
   `captured_frames=reference_frames=120`, `screenshot_count=12`,
   `trace_rows=5243`, `actor_rows=1320`, fetch/opcode/dispatch `555` each,
   handler `1158`, advance `551`, rewind `367`, aligned/fixed-link `555`,
   zero fixed-link mismatch/invalid/misaligned counts,
   `lifecycle_evidence_valid=true`, `final_progress_written=true`,
   `speedmode_ok=true`, `capture_window_complete=true`, and
   `stop_reason=bounded live window complete`. All twelve reference PNGs
   independently decode to `256x224`; the preserved session is
   `2,459,056` bytes. The runner then failed closed because its current
   `ReferenceHeight=240` expected 256x240 while the emitted first PNG was
   256x224; `.incomplete` remains. `original-2` has zero files and native
   capture/video/sheet has zero files. FCEUX stdout/stderr logs are nonempty
   with `exit=0`. The correction changes the PNG contract to 256x224, the
   original sheet to 768x896, keeps the separate original AVI/video contract
   at 256x240, and names static regressions for exact 12-frame inventory,
   stale 256x240/768x960 PNG claims, and expected-versus-actual dimensions.
19. The eighth authorized dirty-tree rehearsal at
   `20260803-050110` is preserved and remains rejected. Before the proof run,
   the static and warning-clean focused `680/24/17` gates passed. `original-1`
   again completed deterministically with `setup_frame=tip_start_frame=3929`,
   `live_start_frame=4101`, `capture_start_frame=4125`, and the inclusive
   window complete through `4244`; final progress was
   `sequence=4245/emu_frame=4243/finished/speedmode=true`, with
   `captured_frames=reference_frames=120`, `screenshot_count=12`,
   `trace_rows=5243`, `actor_rows=1320`, fetch/opcode/dispatch `555` each,
   handler `1158`, advance `551`, rewind `367`, aligned/fixed-link `555`,
   zero mismatch/invalid/misaligned counts, and lifecycle/final-progress/
   capture completion true. The preserved session is `2,459,056` bytes;
   `.incomplete` remains, there is no original-1 contact sheet,
   `original-2` has zero files, and native capture/video/sheet has zero files.
   The runner then failed at `New-ContactSheet` because
   `New-Object Drawing.Bitmap($CellWidth * 3, $CellHeight * 4)` was parsed in
   PowerShell command mode as an `Object[]` multiplication operand, producing
   `Method invocation failed because [System.Object[]] does not contain a
   method named op_Multiply.` Sol reproduced the same RuntimeException with
   the isolated `256/224` statement. The dormant native-sheet constructor at
   the former line ~749 had the same defect with `$NativeWidth/$NativeHeight`.
   Both paths now use typed, separately parenthesized integer dimensions;
   static regression coverage rejects both `New-Object` arithmetic forms and
   requires independent original/native typed 3x4 constructors.
20. The ninth authorized dirty-tree rehearsal at
   `20260803-050610` is preserved and remains rejected. The pre-run static and
   warning-clean focused gates passed. Both original runs completed/pass with
   the established 120-frame evidence/counts. Both original contact sheets
   were created at `768x896`, `76,643` bytes each, with identical SHA-256
   `2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.
   Traces were identical at
   `9EE4DA566665800ECA40E02919AB5323634364620EB71907BCD1901A43A2169D` and
   actors identical at
   `69404667384CC604CE7DD600D33D2F2C232C35FEF6AAE8F3626605E01BF84E6A`.
   The session was `4,898,133` bytes and `.incomplete` remains. Native
   `gameplay-cpu-steering-frame25` pass 1 succeeded at `640x480`,
   `1,229,438` bytes, SHA-256
   `92A3C0009457A688A70A563CE87DE8692C26E3343FB717C3075B63C868260031`,
   with log `exit=0`; the runner then failed with
   `ArgumentOutOfRangeException: Specified argument was out of the range of
   valid values, parameter index.` The exact root was numeric `$Frame` indexing
   into empty `[ordered]@{}` dictionaries, which selects the positional
   `OrderedDictionary` indexer. Sol reproduced `([ordered]@{})[25]='x'` and
   confirmed key `'0025'` succeeds. All native frame hash/detail/repeat-detail
   accesses now use the deterministic `'{0:D4}' -f $Frame` key, with static
   regression coverage for all four accesses and rejection of numeric indexing.
21. The tenth authorized dirty-tree rehearsal at
   `20260803-051045` is preserved and remains rejected. Pre-gates passed. Both
   original runs and sheets, plus all twelve primary and twelve repeat native
   `640x480` frames, passed deterministic per-frame comparison; 24 render logs
   exist. The native contact sheet is `1920x1920`, `261,899` bytes, SHA-256
   `4F3AF89F575572CE80C976B141207D45930DB952C5788C1588C816A7A8160DC9`.
   The primary MP4 encoded successfully at `31,777` bytes, SHA-256
   `0F5D5C3B945A9EB5165632E74A5AE0451059F038728CDE949767BD17BB96345E`;
   the repeat MP4 had not started when the runner failed closed at primary
   ffprobe. Its actual probe was `width=640`, `height=480`,
   `avg_frame_rate=719503/11972`, `nb_frames=12`, and
   `nb_read_frames=12`, versus expected `39375000/655171`. The tiny rate
   difference, about `2.757764e-05` ppm, is default MP4 track-timescale
   quantization, not a missing or extra decoded frame. Sol's isolated encode
   with `-video_track_timescale 39375000` produced exact
   `r_frame_rate=avg_frame_rate=39375000/655171`, `time_base=1/39375000`,
   `duration_ts=7862052`, `duration=0.199671`, and both frame counts `12`,
   at `640x480`; the diagnostic MP4 was `31,777` bytes with SHA-256
   `66632EF630E2798D0908E982502BE854A59E4D7DF054288BB6EE84F6DD85988C`.
   Preserved session size after that diagnostic was `33,517,485` bytes. The
   correction applies this deterministic native proof encoding configuration,
   not ASM/source-pinned gameplay semantics, and records all probe fields in
   the manifest with named static regressions for the timescale, both rate
   fields, time base, and both frame counts.
22. Sol accepted the eleventh `DRAFT_PASS` session at
   `temp-videos/gameplay-lab/cpu-lifecycle/20260803-051716/`, generated UTC
   `2026-08-03T10:17:40.6360836Z`. It has status `draft_pass`, base/head
   `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`, and correctly pending final SHA
   until the worker commit. It contains 102 files, 100 inventoried artifacts,
   36 nonempty runner-metadata logs, zero empty files, no `.incomplete`, and
   `33,652,224` bytes. The manifest is `130,114` bytes with SHA-256
   `457EB6E50BCCBC113B439104143F5834D55C836C9C47E3CC1E2B4D7F6588165A`; the
   summary is `2,936` bytes with SHA-256
   `8A8976146A36B0D8431F0CF04AF392B04DDF9461A5B06CB377A3E7D2E9E690CE`.
   ROM/FCEUX identities are the canonical hashes in `EVIDENCE.md`; the fresh
   pack is `1,397,729` bytes, SHA-256
   `8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`, and
   TGAI-1 is `7,616` bytes/FNV `D6C4DB35`.
23. The accepted original runs have identical trace/actor hashes
   `9EE4DA566665800ECA40E02919AB5323634364620EB71907BCD1901A43A2169D` and
   `69404667384CC604CE7DD600D33D2F2C232C35FEF6AAE8F3626605E01BF84E6A`, frame
   index equality, and identical `768x896` sheets of `76,643` bytes with SHA
   `2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.
   Each has setup/tip `3929`, live `4101`, capture `4125`, 120 frames, 12
   screenshots, 5,243 trace rows, 1,320 actor rows, fetch/opcode/dispatch
   `555` each, 1,158 handlers, 551 advances, 367 rewinds, aligned/fixed `555`,
   zero mismatch/invalid/misaligned counts, and final sequence/emu frame
   `4245/4243` with lifecycle/final-progress/speedmode/capture complete.
24. Sol personally inspected all 5,243 trace rows: 18-69 events per frame,
   fetch/opcode/dispatch/advance at `$8B90/$8BA2/$8BAE/$8FD9`, confidence
   pairs `3,258` exact source/mechanics, `1,158` exact opcode entry, `707`
   deferred mechanics, and `120` inferred labels. Actor CSV has exactly 11
   rows per frame for actors 0-10, slot 10 stream/fixed-link `NA`, and fixed
   link text `05:06:07:08:09:00:01:02:03:04`. Native frames 25-36 and repeats
   are exact at `640x480`; the `1920x1920` sheet is `261,899` bytes with SHA
   `4F3AF89F575572CE80C976B141207D45930DB952C5788C1588C816A7A8160DC9`.
   Both MP4s are `31,777` bytes with SHA
   `66632EF630E2798D0908E982502BE854A59E4D7DF054288BB6EE84F6DD85988C`; both
   probes report H.264, `640x480`, exact rates/time base, duration_ts
   `7862052`, duration `.199671`, and 12/12 metadata/decoded frames.
25. Personal visual inspection found identical intact Bulls-Celtics original
   sheets with clock `3:59 -> 3:57`, readable HUD/court/crowd/sprites, and no
   corruption; inspected original frames are 0001/0006/0012. Native sheet and
   frames 0025/0030/0036 show intact Hawks-Celtics continuity at `3:00`, with
   readable HUD/court/crowd, moving sprites, black margins, and no corruption.
   Team, clock, framing, and scenario differ, so native remains deterministic
   legacy harness/formation regression evidence, not parity or scene
   integration proof. Win32 production smoke passed; formal generated-manifest
   acceptance, clean committed `-RequirePass`, and independent QA remain
   pending.

## Fault ledger

No literal `{detail: bad request}` fault or confirmed equivalent occurred in
this task. Twenty-two recoverable local-tool diagnostics occurred: two ordinary
worker diagnostics from the earlier pass, five current patch-context
diagnostics, one current process/progress assertion patch-context diagnostic,
one current TESTS documentation patch-context diagnostic, and one current
static-assertion quoting diagnostic, plus four fifth-rehearsal trace/static
diagnostics, one ninth-rehearsal native dictionary diagnostic, one tenth-rehearsal
native video cadence diagnostic, and one current
inspection-command regex diagnostic. None changed
files or external state:

| Diagnostic | Raw signature/command | Result |
| --- | --- | --- |
| Patch context mismatch | Raw signature: `apply_patch verification failed: Failed to find expected lines in ...`; command detail unavailable beyond a multi-file clock/proof-doc patch | No patch applied; inventory and file contents were rechecked before smaller patches |
| Thread-list argument validation | Raw signature detail unavailable; command was a thread-list request with an oversized `limit`, then a corrected request with `limit=50` | Rejected request only; no thread/task state, pin, file, or external proof mutation |
| Current static-assertion patch context (count 3) | Sanitized raw signature prefix: `apply_patch verification failed: Failed to find expected lines in <worker>\tools\gameplay-lab\Test-GameplayLab.ps1`; commands attempted to add JSON/log/progress assertions and replace the mojibake detector | No patch applied on each attempt; the file was inspected and the smaller ASCII-safe patches then applied successfully |
| Current static-assertion quoting | Raw signature: `CPU lifecycle failed child commands can escape before nonempty inventoried runner metadata is written.` from `Test-GameplayLab.ps1` after adding the `ErrorActionPreference` matcher | Static assertion only; the regex quote was corrected, then the static suite passed |
| Current Lua/rehearsal documentation patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... TESTS.md`; one bundled Lua/static/LINEAGE/TESTS patch was split after the TESTS context differed | No patch applied; Lua/static/LINEAGE changes were then applied in smaller CPU-owned patches |
| Current cap-rehearsal documentation patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... TESTS.md`; one bundled cap/LINEAGE/TESTS patch was split after the TESTS context differed | No patch applied; the cap and documentation changes were then applied in smaller CPU-owned patches |
| Current process/progress assertion patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... Test-GameplayLab.ps1`; one bundled assertion patch did not match the current static block | No patch applied; the exact blocks were re-read and the process/progress assertions were applied in smaller CPU-owned patches |
| Current process/progress documentation patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... TESTS.md`; one bundled rehearsal-documentation patch did not match the intervening placeholder paragraph | No patch applied; the exact section was re-read and the TESTS rehearsal record was applied at the corrected insertion point |
| Current seventh-rehearsal lineage patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... LINEAGE.md`; the first insertion context did not match the current sixth-rehearsal wording | No patch applied; the exact section was re-read and the seventh-rehearsal record was inserted at the corrected boundary |
| Current trace static-assertion patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... Test-GameplayLab.ps1`; one bundled trace-format/callback/capture assertion patch did not match the current block | No patch applied; the source and static block were re-read before smaller assertions were added |
| Current patch-composition syntax diagnostic | Raw signature: `SyntaxError: missing ) after argument list` while composing a `String.raw` patch containing PowerShell backticks | No file or external state changed; the patch was reissued without the embedded backtick syntax |
| Current trace callback static assertion | Raw signature: `CPU lifecycle registerexec callback errors are not bounded, sanitized, and fail-closed.` from `Test-GameplayLab.ps1` | Static assertion was too broad; its callback text was narrowed to the registered callback block, then the suite passed |
| Current outer-xpcall static assertion | Raw signature: `CPU lifecycle Lua frameadvance is enclosed by a yield-crossing outer xpcall.` from `Test-GameplayLab.ps1` | The old negative matched the new callback xpcall; it was narrowed to `xpcall(function() while not stopped do`, then the suite passed |
| Current stale-lineage correction patch context | Raw signature: `apply_patch verification failed: Failed to find expected lines in ... LINEAGE.md`; the first narrow replacement context did not match the already-updated seventh-rehearsal lineage text | No patch applied; the exact current requirement row was re-read and corrected directly |
| PowerShell Core command unavailable | Raw signature: `pwsh : The term 'pwsh' is not recognized as the name of a cmdlet, function, script file, or operable program.` | The static suite was rerun successfully with `powershell.exe`; no file or external state changed |
| Initial path-boundary audit matched an allowed placeholder | Raw signature: the first absolute-path scan returned the generic `TESTS.md` command example `C:\path\to\...` and exited nonzero | The context-aware audit allowed that non-private placeholder and passed; no file or external state changed |
| Eighth rehearsal bitmap constructor | Raw signature: `Method invocation failed because [System.Object[]] does not contain a method named op_Multiply.`; isolated reproduction used the original `256/224` constructor form | Both original and native `New-Object Drawing.Bitmap` arithmetic forms were replaced with typed `[Drawing.Bitmap]::new` constructors; no FCEUX/private proof was rerun |
| Ninth rehearsal native frame dictionary | Raw signature: `ArgumentOutOfRangeException: Specified argument was out of the range of valid values, parameter index.`; isolated reproduction was `([ordered]@{})[25]='x'` | All four native dictionary accesses now use the canonical D4 string key; no FCEUX/private proof was rerun |
| Tenth rehearsal native video cadence | Raw signature: primary ffprobe returned `avg_frame_rate=719503/11972` instead of exact `39375000/655171`; dimensions and both frame counts were correct | Added `-video_track_timescale 39375000`; validation now requires `r_frame_rate`, `avg_frame_rate`, `time_base`, `nb_frames`, and `nb_read_frames` exact values |
| Current inspection-command regex | Raw signature: `rg: regex parse error: (?:\|\|\) unclosed group` from an over-escaped read-only inspection pattern | The inspection was reissued with a literal alternation; no file or external state changed |

None changed files, created a task, altered pins, or caused an external retry.
The research Lunas likewise report zero bad-request faults, no mutation, and
no commit.

## Fault/revision register

| Finding | Correction | Status |
| --- | --- | --- |
| `$06CB` was described as unreconstructed | fixed startup link is source-pinned and kept separate from dynamic state | corrected |
| shared deferred-handler comment implied no transport | comment now says effect deferred and transport selected below | corrected in draft |
| opcode 10 fell through to +5 | same-offset deferred bounded executor plus golden | accepted core |
| goto budget could be confused with a tick loop | `$0005 -> $0000 -> $0005` and budget-1 goldens | accepted core |
| route invalid outputs could be overwritten | bad tag and controller-slot transaction goldens | accepted core |
| formation tolerance risk | exact 46 pinned rows; 46/47 rejected | accepted core |
| reference capture could begin before tip/clock | setup/tip/clock-running gate and post-clock delay | corrected in draft |
| empty reference window could pass | positive fetch/opcode/dispatch/handler/advance, aligned-stream, and fixed-link gates | corrected in draft |
| dispatcher address was shifted to `$8BE1` | `$8BAE` indirect dispatch; `$8BE1` named as opcode-22 handler | corrected in draft |
| handler-table data was treated as executable | `$8BB1/$8BC9` retained as static anchors only | corrected in draft |
| actor slot 10 serialized `$06D5` | slot 10 fixed-link field is `NA`; fixed-link validation covers slots 0-9 | corrected in draft |

## Rejected proof paths and negative regressions

Every proof rejection below is paired with a named fail-closed assertion in
`tools/gameplay-lab/Test-GameplayLab.ps1` and, where runtime data exists, a
matching Lua/PowerShell validator. These are negative requirements, not claims
that a private FCEUX run has occurred.

| Rejected path | Corrected contract | Exact static/runtime regression |
| --- | --- | --- |
| Pre-tip mode/screen was falsely accepted as live | Require stopped-clock observation, running clock, and restored canonical setup/invariants | `CPU lifecycle running-clock/live-invariant gate regressed to pre-tip mode/screen detection`; Lua `running_clock_live_seen` and `live_setup_valid()` gate capture |
| Zero-dispatch window was accepted | Require positive fetch/opcode/dispatch/handler/advance, aligned-stream, and fixed-link evidence | `CPU lifecycle proof does not fail closed on missing source-pinned execution evidence`; runner positive `TryParse` evidence gates |
| `$8BE1` was mislabeled as dispatcher | `$8BAE` is indirect dispatch; `$8BE1` is opcode 22 handler | `CPU dispatcher/table distinction...`; map `dispatch_cpu = 0x8BAE`, `name="opcode_22_handler"` |
| `$8BB1/$8BC9` data was registered/treated as hooks | Keep table addresses as static anchors only | Same dispatcher/table assertion rejects `{ address=0x8BB1` and `{ address=0x8BC9` |
| Slot 10 received fabricated stream/fixed-link data | Serialize both slot-10 fields as `NA`; validate only `$06CB[0..9]` | `CPU lifecycle actor slot 10 incorrectly serializes stream or fixed-link data`; Lua slot-10 CSV regression |
| Exact and inferred labels were conflated | Split exact source-pinned address evidence from bounded semantic confidence | `CPU lifecycle exact address evidence and semantic label confidence are not split`; Lua per-hook columns and map classifications |
| Explicit low-address handler hooks suppressed generated handler-kind evidence | Merge by address but classify all overlapping source entries as `kind="handler"` and use a handler-address set for command fields | `CPU lifecycle handler deduplication...`; map checks `$8C40/$8CD0/$8E4F/$9172/$8BE1` and Lua dedup/field test |
| Buffered metadata/trace files caused a false watchdog stall/pass | Use a periodically atomically published `progress.txt` sentinel from startup through capture | `CPU lifecycle boot progress sentinel/watchdog...`; runner watchdog reads only progress |
| Missing, empty, or wrong-count screenshots were accepted | Require exactly `reference-frame-0001.png` through `0012.png`, 256x224, nonempty hashes, status count 12; reject empty fingerprints | `CPU lifecycle incomplete-sentinel, frame inventory...` and Lua/runner screenshot gates |
| The 4200-frame/4096-row capacity was infeasible for the observed schedule | Use empirical deterministic `max_frames=4320` and `trace_rows=8192`, retain 8 MiB/64 MiB caps, and fail fast on `capture_start + frames - 1 > max_frames` | `CPU lifecycle empirical capacity...` and `CPU lifecycle inclusive capture-window feasibility fail-fast is missing` |
| FCEUX reference PNGs were treated as 256x240 and sheets as 768x960 | Lock emitted PNGs to 256x224 and original sheets to 768x896; keep original AVI/video separately declared 256x240 | `CPU lifecycle original/native contact-sheet dimensions...`; manifest/proof raster-versus-video assertions |
| Pending/control-plane SHA metadata was presented as product proof | No product-acceptance SHA field; draft is `draft_pass`; final proof uses current Git HEAD and rejects dirty/pending metadata under `-RequirePass` | `CPU lifecycle Git cleanliness/final-SHA/pending-metadata...`; runner `Get-GitState`/`$GitState.head` assertion |
| Empty or uninventoried logs were accepted | Every log gets sanitized runner metadata and silent-stream output, then remains required/nonempty in the inventory | `CPU lifecycle log nonempty/inventory contract is missing`; `Add-ProcessLogMetadata`/`Get-ArtifactInventory` |
| Focused wrapper named arguments were bound positionally | Run the focused script through nested `powershell.exe -NoProfile -ExecutionPolicy Bypass -File` with explicit `-RomPath`, `-ProjectRoot`, and `-Build` arguments | `CPU lifecycle focused wrapper does not transport named parameters through a nested PowerShell process` |
| Child wrapper failure escaped before `cpu-focused.log` existed | Catch invocation failures, capture the error as raw log content, write runner metadata, and return code 1 before inventory | `CPU lifecycle failed child commands can escape before nonempty inventoried runner metadata is written` |
| `FCEU.frameadvance` crossed an outer `xpcall` C-call boundary | Keep only the non-yielding per-frame `xpcall(execute_frame, ...)`; run the top-level frame loop and `FCEU.frameadvance()` outside protected calls | `CPU lifecycle Lua frameadvance is enclosed by a yield-crossing outer xpcall` |
| Output cap falsely triggered below 64 MiB | Parenthesize every `Get-SessionBytes` result before `-gt`: one run-path check and two whole-session checks | `CPU lifecycle session output cap comparison is unparenthesized or incomplete` |
| Blank FCEUX exit text came from reading process status before stream completion/cache | Wait without a timeout in `finally`, cache exit before disposal, then decide from the cached value after `finally` | `CPU lifecycle process status is read before cached post-WaitForExit bookkeeping or after disposal` |
| Progress reader could block Lua rename and publisher treated one collision as permanent | Use bounded shared FileStream reads and three remove/rename attempts with `.tmp`/finished validation | `CPU lifecycle progress reader uses a blocking whole-file API or lacks rename-safe bounded sharing`; `CPU lifecycle progress publisher uses a single-shot remove/rename instead of bounded retry` |
| Trace header had 30 fields but 31 type-misaligned conversions | Use one 30-column header, one 30-conversion format, and 30 indexed values; statically verify fixed-link and confidence positions | `CPU lifecycle trace header, format conversion, value count, or fixed-link/confidence positions are misaligned` |
| `registerexec` error escaped outside the frame-step protection | Wrap mapper gate and `record_hook` in bounded non-yielding `xpcall`, sanitize the error, and defer abort to the next frame | `CPU lifecycle registerexec callback errors are not bounded, sanitized, and fail-closed` |
| Pre-window callback rows were accepted while `captured == 0` | Require `frame >= capture_start_frame` and `captured` in the bounded capture range before recording | `CPU lifecycle trace evidence is recorded before the defined capture window` |
| Pass carried unavailable or nondeterministic video | `-RequirePass` implies `-RequireVideo`; primary/repeat videos are JSON-probed and SHA-equal | `CPU lifecycle RequirePass/video and deterministic repeat-video contract is missing` |
| Original/native contact-sheet review was incomplete | Two 768x896 original sheets and one 1920x1920 native sheet are required; original hashes must match | `CPU lifecycle original/native contact-sheet dimensions or equality contract is missing` |
| Live-common side/actor invariant was incomplete | Require `defense_side == 1 - offense_side` and both actor selectors 0..9 | Running-clock/live-invariant assertion plus Lua live gate |
| Slot 10 stream offset was formatted as unsigned hex | Emit `NA` for both slot-10 stream and fixed-link fields | `CPU lifecycle actor slot 10 incorrectly serializes stream or fixed-link data` |
| Candidate/switch/shot labels inherited exact confidence | Every hook carries explicit exact-address and bounded semantic label confidence | `CPU lifecycle candidate/switch/shot label confidence is not explicitly bounded` |

No bad-request retry or external-tool retry has been performed in this phase.
Any future retry must first reconcile inventory, branch, HEAD, cleanliness, and
the proof/output directory.

## Pins and ownership

The worker is pinned to the CPU-owned path families only. Existing dirty legacy
worktrees, especially the TIP input worker, are preserved. The Sol owns final
acceptance, source-map compatibility, master integration, and formal final
proof. Sol's draft source/trace/visual inspection is complete and recorded in
the accepted eleventh-session entries above; formal independent QA and clean
`-RequirePass` acceptance remain pending.
