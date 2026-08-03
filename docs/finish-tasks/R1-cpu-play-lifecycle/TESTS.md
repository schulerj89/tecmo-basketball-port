# R1 CPU lifecycle tests and proof commands

## Prerequisites

- Windows PowerShell in the Luna worktree.
- Canonical local Rev1 ROM with SHA-256
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
- The repository's existing build toolchain and bundled warning scanner.
- Private FCEUX 2.6.6 with SHA-256
  `F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B` only
  for the optional proof command; it is not required for the native focused
  test.

## Latest focused command

The warning-clean focused gate was rerun for this draft with:

```powershell
$env:TECMO_SKIP_SHORTCUT='1'
.\tools\Run-GameplayCpuSteeringTests.ps1 -Build `
  -RomPath 'C:\path\to\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
```

Recorded result:

```text
TGAI-1 focused tests passed: exact Rev1 importer and ten source spans plus seven lifecycle anchor/table spans, 680 aligned commands, 24 handlers, eight exact direction codes, deterministic ten-coordinate/context harness, transactional TGMO direction/movement composition, strict provenance/dependency/parser/input mutations, 17 ROM mutations (7 lifecycle anchor/table), bounded live scene adapter enabled
```

The renewal static suite was also rerun after the exact stopped-clock gate,
progress-watchdog grace, screenshot inventory, and rejected-path matrix changes:

```powershell
$env:TECMO_SKIP_SHORTCUT='1'
.\tools\gameplay-lab\Test-GameplayLab.ps1
```

Result:
`GAMEPLAY LAB STATIC TEST PASS: closed profiles, CPU lifecycle proof surface,
read-only controller policy, exact revisions, bounded output, point/velocity
evidence, fail-closed shot/lifecycle evidence, neutral cleanup`.

### Rejected dirty-tree rehearsal

The authorized dirty-tree rehearsal at ignored output
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-035113/` is preserved as a
negative proof artifact. Native build succeeded, then the focused wrapper was
invoked through PowerShell array splatting and received `-RomPath` as a
positional `ProjectRoot` value; `Resolve-Path` failed before FCEUX. Parent
`ErrorActionPreference=Stop` also escaped the child failure before
`cpu-focused.log` metadata was written, leaving `.incomplete` and
`native-build.log` only. The corrected runner uses nested `powershell.exe`
with `-NoProfile -ExecutionPolicy Bypass -File -RomPath -ProjectRoot -Build`,
and `Invoke-Logged` catches failures, writes nonempty runner metadata, and
returns a nonzero result for inventory validation.

### Rejected FCEUX startup rehearsal

The second authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-035908/`. Named wrapper and
logging passed, including nonempty `cpu-focused.log`, asset-pack, and
self-test logs, but `original-1` missed the five-second startup sentinel.
After diagnostic contention was removed, the solo diagnostic at
`temp-videos/gameplay-lab/cpu-lifecycle/diagnostic-visible-startup-solo/status.txt`
exited with `sequence=1`, `emu_frame=0`, `result=abort`, all lifecycle and
capture counts zero, and:

```text
stop_reason=uncaught Lua error: attempt to yield across metamethod/C-call boundary
```

The cause was the outer `xpcall` around the loop containing
`FCEU.frameadvance()`. The outer wrapper is removed; only the non-yielding
per-frame `xpcall(execute_frame, ...)` remains. The named static regression
`CPU lifecycle Lua frameadvance is enclosed by a yield-crossing outer xpcall`
now fails if that structure returns.

### Rejected output-cap rehearsal

The third authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-040852/`. It reached
startup/progress at `sequence=40`, `emu_frame=38`, `stage=boot`, then falsely
reported `exceeded 64 MiB output cap`. The exact preserved tree size was
`1,405,077` bytes; the cap is `67,108,864` bytes. PowerShell parsed each
`Get-SessionBytes` call as command arguments because the function result was
not parenthesized before `-gt`. All three comparisons are now corrected. The
named static regression `CPU lifecycle session output cap comparison is
unparenthesized or incomplete` requires exactly one `$Path` and two
`$OutputRoot` parenthesized comparisons and rejects the broken form.

### Rejected process/progress-publication rehearsal

The fourth authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-041248/`. The runner first
reported `FCEUX 'original-1' exited .` with blank exit text; the preserved
FCEUX logs later show runner metadata with `exit=0`. Its final progress record
was `sequence=81`, `emu_frame=79`, `stage=finished`, and
`speedmode_ok=true`; its status was `result=abort` with
`stop_reason=CPU lifecycle progress sentinel could not publish`.

The process failure was an exit-code check before the final redirected-stream
wait/refresh/cache path. The corrected `Invoke-ReferenceRun` initializes a
cached exit variable, calls parameterless `WaitForExit()` in `finally`, reads
and logs the cached exit code before disposal, and makes the nonzero decision
after `finally`; watchdog exceptions still propagate naturally. The progress
failure used `[IO.File]::ReadAllText`, which can deny Lua's Windows
remove/rename, and a single-shot remove/rename. The corrected reader uses a
bounded `FileStream` opened with `FileShare.ReadWrite -bor FileShare.Delete`
and deterministic disposal. The Lua writer captures the remove result and
retries remove/rename three times, while the `.tmp` and finished-progress
checks remain fail-closed.

The named static regressions are `CPU lifecycle process status is read before
cached post-WaitForExit bookkeeping or after disposal`, `CPU lifecycle
progress reader uses a blocking whole-file API or lacks rename-safe bounded
sharing`, and `CPU lifecycle progress publisher uses a single-shot
remove/rename instead of bounded retry`. The existing boot-progress assertion
also remains fail-closed; the reader assertion rejects `ReadAllText` inside
`Get-ProgressSnapshot` and the publisher assertion requires the bounded retry
contract.

### Rejected live-callback rehearsal

The fifth authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-042609/`. It passed setup,
tip, stopped-clock, and live gates, then stalled at
`sequence=4101`, `emu_frame=4099`, `stage=running-clock-live`, with
`setup/tip/live=true`, `captured_frames=0`, and `speedmode=true`. No status
file was produced and the trace/actor bodies were empty. The clean visible
diagnostic at `diagnostic-visible-live-callback` reproduced the same marker;
FCEUX displayed GUI-only `Lua run error (null) (null)` with an empty Lua
console at the first live-enabled exec callback. No FCEUX instance remains.

The trace header contained 30 columns but the formatter contained 31
conversions and had type drift: `fixed_link_text()` reached `%02X`, address
confidence reached `%d`, and a final `%s` had no supplied value. The callback
was outside the per-frame `xpcall`, allowing the GUI modal/stall. In addition,
`record_hook` accepted `live_seen` and a nonnegative `capture_start_frame`
while `captured` was still zero, so pre-window callbacks could become
evidence. The correction uses one 30-column header, 30 type-aligned format
conversions, and 30 indexed values; every `registerexec` callback now has a
bounded non-yielding `xpcall` that sanitizes into `deferred_failure`; and
`record_hook` requires `frame >= capture_start_frame` plus
`captured` in `1..reference_window.frames`.

Named static regressions are `CPU lifecycle trace header, format conversion,
value count, or fixed-link/confidence positions are misaligned`, `CPU
lifecycle registerexec callback errors are not bounded, sanitized, and
fail-closed`, and `CPU lifecycle trace evidence is recorded before the defined
capture window`.

### Rejected capture-cap rehearsal

The sixth authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-043857/`. It executed
cleanly and produced fail-closed status after passing the setup, tip,
stopped-clock, and live gates. The observed schedule was
`setup_frame=3929`, `tip_start=3929`, `live_start=4101`,
`capture_start=4125`, and `max_frames=4200`; it reached only
`captured=76/120` and `screenshots=8/12`. The final progress was
`sequence=4200`, `emu_frame=4198`, `stage=finished`, `speedmode=true`, with
`stop_reason=maximum frame cap reached before complete live window`.
Positive source-pinned evidence was already valid: `trace_rows=3163`,
`actor_rows=836`, fetch/opcode/dispatch `327` each, handler `708`, advance
`325`, aligned `327`, fixed-link `327`, and zero mismatch/invalid/misaligned
counts.

The bounds were internally infeasible: the inclusive 120-frame window ends at
`capture_start + frames - 1 = 4244`, beyond 4200. The observed trace density
also projects roughly 5061 rows for a complete window, above the prior 4096
row cap. The correction sets the single empirical deterministic proof-surface
capacity to `max_frames=4320` and `trace_rows=8192`, retaining the 8 MiB text
and 64 MiB session caps. At live transition Lua now computes the inclusive
capture end and aborts immediately if it exceeds the selected session max.
These capacities are schedule/runtime bounds, not ASM/source-pinned
semantics. Named regressions lock map/runner equality, the inclusive runway
formula, `trace_rows=8192`, and reject old `4200`/`4096` claims.

### Rejected screenshot-raster rehearsal

The seventh authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-044527/`. The renewed
`4320/8192` capacity worked: `original-1` completed/pass with
`setup_frame=tip_start_frame=3929`, `live_start_frame=4101`,
`capture_start_frame=4125`, capture through inclusive frame `4244`,
`captured_frames=reference_frames=120`, `screenshot_count=12`,
`trace_rows=5243`, `actor_rows=1320`, fetch/opcode/dispatch `555` each,
handler `1158`, advance `551`, rewind `367`, aligned/fixed-link `555`, zero
mismatch/invalid/misaligned counts, `lifecycle_evidence_valid=true`,
`final_progress_written=true`, `speedmode_ok=true`,
`capture_window_complete=true`, and `stop_reason=bounded live window complete`.
All twelve `reference-frame-0001.png` through `0012.png` independently decode
to `256x224`; the preserved session totals `2,459,056` bytes. FCEUX
stdout/stderr logs are nonempty with `exit=0`.

The runner then failed closed during first-run artifact validation because
`ReferenceHeight=240` expected 256x240 while the emitted first PNG was
256x224. `.incomplete` remains; `original-2` has zero files, and native
capture/video/sheet has zero files. The correction changes the FCEUX PNG
contract to `256x224` and the original 3x4 contact sheet to `768x896`, while
keeping the separate original AVI/video contract at `256x240`. Named static
regressions lock the exact 12-frame inventory, reject stale current
256x240/768x960 PNG claims, preserve the distinct video contract, and require
dimension failures to report both actual and expected sizes.

The literal local ROM path above is intentionally represented as a placeholder
in committed documentation. The real private path is never committed.

### Rejected contact-sheet-constructor rehearsal

The eighth authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-050110/`; no FCEUX/private
proof is being rerun for this correction. The pre-run static and warning-clean
focused `680/24/17` gates passed. `original-1` completed/pass deterministically
with `setup_frame=tip_start_frame=3929`, `live_start_frame=4101`,
`capture_start_frame=4125`, and the inclusive window complete through `4244`.
Final progress was `sequence=4245/emu_frame=4243/finished/speedmode=true`;
`captured_frames=reference_frames=120`, `screenshot_count=12`,
`trace_rows=5243`, `actor_rows=1320`, fetch/opcode/dispatch `555` each,
handlers `1158`, advance `551`, rewind `367`, aligned/fixed-link `555`, and
all mismatch/invalid/misaligned counts were zero. Lifecycle, final-progress,
and capture completion were true. The preserved session totals `2,459,056`
bytes; `.incomplete` remains, there is no original-1 contact sheet,
`original-2` has zero files, and native capture/video/sheet has zero files.

The runner then failed in `New-ContactSheet`: PowerShell parsed
`New-Object Drawing.Bitmap($CellWidth * 3, $CellHeight * 4)` in command mode,
and raised `Method invocation failed because [System.Object[]] does not contain
a method named op_Multiply.` Sol reproduced that exact RuntimeException with
the isolated `256/224` statement. The dormant native-sheet constructor near
line ~749 used the same broken arithmetic form with `$NativeWidth` and
`$NativeHeight`. Both original and native paths now construct typed
`[Drawing.Bitmap]::new([int](width * 3), [int](height * 4))` dimensions with
deterministic disposal preserved. A named static regression independently
requires both typed 3x4 constructors and rejects any `New-Object
Drawing.Bitmap(` arithmetic form.

### Rejected native-frame-key rehearsal

The ninth authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-050610/`; no FCEUX/private
proof is being rerun for this correction. The pre-run static and focused gates
passed. Both original runs completed/pass with the established 120-frame
evidence/counts. Both contact sheets were created at `768x896`, `76,643`
bytes each, with identical SHA-256
`2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.
Trace outputs were identical at
`9EE4DA566665800ECA40E02919AB5323634364620EB71907BCD1901A43A2169D` and
actor outputs identical at
`69404667384CC604CE7DD600D33D2F2C232C35FEF6AAE8F3626605E01BF84E6A`.
The preserved session totals `4,898,133` bytes and `.incomplete` remains.

Native `gameplay-cpu-steering-frame25` pass 1 succeeded at `640x480`, with
`1,229,438` bytes, SHA-256
`92A3C0009457A688A70A563CE87DE8692C26E3343FB717C3075B63C868260031`, and
log `exit=0`. The runner then failed with
`ArgumentOutOfRangeException: Specified argument was out of the range of valid
values, parameter index.` The native frame dictionaries are `[ordered]@{}`;
numeric `$Frame=25` indexing selects the positional `OrderedDictionary`
indexer, while the isolated `([ordered]@{})[25]='x'` reproduction fails and
key `'0025'` succeeds. The correction introduces
`$FrameKey = '{0:D4}' -f $Frame` and uses it for primary hashes, primary
details, repeat details, and repeat-hash comparison. Static coverage requires
all four D4 accesses and rejects numeric indexing independently.

### Rejected native-video-cadence rehearsal

The tenth authorized dirty-tree rehearsal is preserved at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-051045/`; no FCEUX/private
proof is being rerun for this correction. Pre-gates passed. Both original runs
and sheets, and all 12 primary plus 12 repeat native `640x480` frames, passed
deterministic per-frame comparison; 24 render logs exist. The native contact
sheet is `1920x1920`, `261,899` bytes, SHA-256
`4F3AF89F575572CE80C976B141207D45930DB952C5788C1588C816A7A8160DC9`.
The primary MP4 encoded successfully at `31,777` bytes, SHA-256
`0F5D5C3B945A9EB5165632E74A5AE0451059F038728CDE949767BD17BB96345E`; the
repeat MP4 did not start. The runner failed closed at primary ffprobe.

The actual primary probe was `width=640`, `height=480`,
`avg_frame_rate=719503/11972`, `nb_frames=12`, and `nb_read_frames=12`, while
the expected rate was `39375000/655171`. The approximately
`2.757764e-05` ppm difference is default MP4 track-timescale quantization, not
missing or extra decoded frames. Sol's isolated diagnostic encode with
`-video_track_timescale 39375000` produced exact
`r_frame_rate=avg_frame_rate=39375000/655171`, `time_base=1/39375000`,
`duration_ts=7862052`, `duration=0.199671`, `nb_frames=12`,
`nb_read_frames=12`, and `640x480`; it was `31,777` bytes with SHA-256
`66632EF630E2798D0908E982502BE854A59E4D7DF054288BB6EE84F6DD85988C`.
Preserved session size after the diagnostic was `33,517,485` bytes. The
correction is deterministic native proof encoding configuration, not
ASM/source-pinned gameplay semantics. Static coverage now requires the shared
track-timescale flag, both exact rate fields, exact time base, both frame
counts, manifest probe fields, and repeat SHA equality.

### Accepted eleventh draft-pass evidence

Sol accepted the `DRAFT_PASS` session at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-051716/`, generated UTC
`2026-08-03T10:17:40.6360836Z`. It has base/head
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`; final SHA was correctly pending
at generation, and the resulting worker implementation/evidence commit is
`db5a043244361b3e9bbab2e154c7f14e4a4a5014`. The session contains 102 files,
100 inventoried artifacts, 36
nonempty logs with complete runner metadata, zero empty files, no `.incomplete`,
and `33,652,224` bytes. Manifest: `130,114` bytes,
`457EB6E50BCCBC113B439104143F5834D55C836C9C47E3CC1E2B4D7F6588165A`;
summary: `2,936` bytes,
`8A8976146A36B0D8431F0CF04AF392B04DDF9461A5B06CB377A3E7D2E9E690CE`.

The exact ROM/FCEUX identities remain
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` and
`F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`.
The fresh pack is `1,397,729` bytes,
`8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141`, and
TGAI-1 is `7,616` bytes/FNV `D6C4DB35`. Original trace/actor hashes are
`9EE4DA566665800ECA40E02919AB5323634364620EB71907BCD1901A43A2169D` and
`69404667384CC604CE7DD600D33D2F2C232C35FEF6AAE8F3626605E01BF84E6A`; both
sheets are identical `768x896`, `76,643` bytes,
`2EE377C3A97A2C415ED223A4E81C468230BCC6E4A987BABFC7F622E928B22B37`.
Both original runs have setup/tip `3929`, live `4101`, capture `4125`, 120
frames, 12 screenshots, trace `5243`, actors `1320`, fetch/opcode/dispatch
`555` each, handlers `1158`, advance `551`, rewind `367`, aligned/fixed `555`,
zero mismatch/invalid/misaligned counts, and final sequence/emu frame
`4245/4243` with all completion gates true.

Sol inspected the full trace and found 30 columns, 5,243 rows, 18-69 events
per frame, exact fetch/opcode/dispatch/advance addresses `$8B90/$8BA2/$8BAE/$8FD9`,
and confidence totals `3,258` exact source/mechanics, `1,158` exact opcode
entry, `707` deferred mechanics, and `120` inferred labels. Actor rows are
exactly 11 per frame for slots 0-10; slot 10 stream/fixed-link is `NA`, and
fixed-link text is `05:06:07:08:09:00:01:02:03:04`.

Native frames 25-36 and exact repeats are `640x480`; the native sheet is
`1920x1920`, `261,899` bytes,
`4F3AF89F575572CE80C976B141207D45930DB952C5788C1588C816A7A8160DC9`.
Primary/repeat MP4s are each `31,777` bytes with SHA-256
`66632EF630E2798D0908E982502BE854A59E4D7DF054288BB6EE84F6DD85988C` and
independent probes report H.264, `640x480`, exact rate/time-base,
`duration_ts=7862052`, duration `.199671`, and 12 decoded/metadata frames.
Personal visual inspection found intact identical Bulls-Celtics original
sheets at `3:59 -> 3:57` and intact Hawks-Celtics native continuity at `3:00`;
HUD, court, crowd, sprites, framing, and black margins are readable with no
corruption. The teams/scenarios differ, so native remains continuity evidence,
not parity or scene-integration proof. Win32 production smoke also passed.

At generation this draft record correctly left formal generated-manifest
acceptance, a clean committed `-RequirePass` run, and independent QA pending.
That historical state is superseded by the formal closure below.

## Formal clean proof and independent QA closure

The clean formal proof passed at
`temp-videos/gameplay-lab/cpu-lifecycle/20260803-053244/`, generated UTC
`2026-08-03T10:33:08.4555394Z`, with `status=pass`, base
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`, and proven code/doc HEAD
`8be7a9f9a11d43e68b090a98af122758885931fd` on the pinned worker branch. The
tracked and nonignored tree was clean, personal inspection was complete, and
no pending metadata remained. The manifest is
`E7C9E6C9210D398DADC82715779A1389DF881643D109A0FDB091EBAFA523254A`; the
summary is
`78C91AAF981C075BF9088EE4618EBB73CDB740DF08E12B5AC1D5E125C5419252`.
The session contains 102 files, 100 inventoried artifacts, 36 nonempty logs,
zero empty files, and `33,650,575` bytes. Its exact ROM, FCEUX, fresh-pack,
TGAI, original, native, and video identities are recorded in `EVIDENCE.md`.

The formal proof contract used the following environment and exact PowerShell
invocation, with the canonical Rev1 ROM and FCEUX 2.6.6 paths supplied
privately:

```powershell
$env:TECMO_SKIP_SHORTCUT='1'
$env:TECMO_CPU_LIFECYCLE_PERSONAL_SOL_INSPECTION='complete'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\gameplay-lab\Run-GameplayCpuLifecycleProof.ps1 `
  -RomPath '<CANONICAL_REV1_ROM.nes>' `
  -FceuxPath '<FCEUX_2.6.6.exe>' `
  -Build -RequireVideo -RequirePass
```

Independent final QA was performed by thread
`019fc628-0b32-7e83-b969-b41990b36e9b`, `gpt-5.6-luna/max`, repinned for
read-only final QA. It accepted `8be7a9f9a11d43e68b090a98af122758885931fd`
with no P0/P1 findings; the only finding was this bounded docs-only closure.
The exact QA commands passed:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\gameplay-lab\Test-GameplayLab.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Run-GameplayCpuSteeringTests.ps1 `
  -Build -RomPath '<CANONICAL_REV1_ROM.nes>' `
  -ProjectRoot '<WORKER_PROJECT_ROOT>'
```

The QA result was `680` commands, `24` handlers, and `17` ROM mutation
rejections, with a clean tree, zero bad-request faults, no mutation, and no
FCEUX/private proof. Formal acceptance is complete at `8be7...`; this worker
turn adds only task documentation, whose terminal commit SHA is reported in
the handoff because it cannot be self-embedded. Dynamic policy/workspace
effects, normal scene integration, and one-to-one native/original parity
remain deferred as stated in `PROOF.md`.

## Static lab command

```powershell
.\tools\gameplay-lab\Test-GameplayLab.ps1
```

This checks both existing closed shot profiles and the separate CPU lifecycle
map/Lua/runner for exact revision locks, mapper-gated CPU hooks, controller-only
input, complete neutral pads, no RAM writes/cheats/savestates, bounded output,
native render commands, and fail-closed cleanup. Result for this draft:
`GAMEPLAY LAB STATIC TEST PASS: closed profiles, CPU lifecycle proof surface,
read-only controller policy, exact revisions, bounded output, point/velocity
evidence, fail-closed shot/lifecycle evidence, neutral cleanup`.

The static checks also reject `$8BB1/$8BC9` as runtime hooks, require `$8BAE`
as the indirect dispatcher and `$8BE1` as opcode 22's handler, require the
setup/tip/exact-stopped-clock/running-clock/live-common gate, positive
source-pinned lifecycle evidence, split per-hook confidence labels, nonempty
inventoried logs, final progress, exact original/native contact sheets, repeat
video equality, `nb_read_frames`, and clean tracked/untracked `-RequirePass`
state. No additional FCEUX/private proof was launched in this turn; the
accepted eleventh `DRAFT_PASS` evidence is recorded below.

### Rejected proof-path regression matrix

The following negative paths are intentionally fail-closed. The assertion names
are also recorded in `LINEAGE.md` and must remain visible in the static suite:

| Rejected path | Required assertion or validator |
| --- | --- |
| Pre-tip mode/screen falsely accepted as live | `CPU lifecycle running-clock/live-invariant gate regressed to pre-tip mode/screen detection` |
| Zero-dispatch window accepted | `CPU lifecycle proof does not fail closed on missing source-pinned execution evidence` |
| `$8BE1` mislabeled dispatcher | `CPU dispatcher/table distinction is missing or static table bytes are registered as hooks` |
| `$8BB1/$8BC9` treated as executable hooks | Same dispatcher/table assertion; static anchors must not be registered |
| Fabricated slot-10 fixed link | `CPU lifecycle actor slot 10 incorrectly serializes stream or fixed-link data` |
| Exact/inferred label conflation | `CPU lifecycle exact address evidence and semantic label confidence are not split` |
| Explicit handler hooks suppress handler-kind evidence | `CPU lifecycle handler deduplication does not preserve handler-kind evidence or command fields` |
| Buffered trace/metadata files used as watchdog progress | `CPU lifecycle boot progress sentinel/watchdog is missing or uses buffered files` |
| Missing, empty, or wrong-count screenshots | `CPU lifecycle incomplete-sentinel, frame inventory, or asset-pack bounds contract is missing` plus `Get-ReferenceFrameRecords`/`Get-FileFingerprint` |
| Pending/control-plane SHA presented as product proof | `CPU lifecycle Git cleanliness/final-SHA/pending-metadata contract is missing` |
| Empty/uninventoried logs | `CPU lifecycle log nonempty/inventory contract is missing`; `Add-ProcessLogMetadata` and `Get-ArtifactInventory` |
| Focused wrapper named parameters bound positionally | `CPU lifecycle focused wrapper does not transport named parameters through a nested PowerShell process` |
| Failed child command escaped before its log was written | `CPU lifecycle failed child commands can escape before nonempty inventoried runner metadata is written` |
| `FCEU.frameadvance` enclosed by outer `xpcall` | `CPU lifecycle Lua frameadvance is enclosed by a yield-crossing outer xpcall` |
| Output cap falsely triggered below 64 MiB | `CPU lifecycle session output cap comparison is unparenthesized or incomplete` |
| Blank FCEUX exit text from pre-cache process-status read | `CPU lifecycle process status is read before cached post-WaitForExit bookkeeping or after disposal` |
| Progress reader blocks Lua rename or publisher gives up after one attempt | `CPU lifecycle progress reader uses a blocking whole-file API or lacks rename-safe bounded sharing`; `CPU lifecycle progress publisher uses a single-shot remove/rename instead of bounded retry` |
| 30-column trace had 31 conversions/type drift | `CPU lifecycle trace header, format conversion, value count, or fixed-link/confidence positions are misaligned` |
| Live callback error escaped to the FCEUX GUI modal | `CPU lifecycle registerexec callback errors are not bounded, sanitized, and fail-closed` |
| Pre-window lifecycle callback was counted as evidence | `CPU lifecycle trace evidence is recorded before the defined capture window` |
| Inclusive live window exceeded the 4200-frame session cap | `CPU lifecycle inclusive capture-window feasibility fail-fast is missing`; `CPU lifecycle empirical capacity is stale, mismatched, or still claims the old 4200/4096 bounds` |
| Trace density exceeded the 4096-row cap | Same empirical-capacity assertion; map and runner must use `trace_rows=8192` |
| FCEUX PNG raster was treated as 256x240 | `CPU lifecycle original/native contact-sheet dimensions or equality contract is missing`; runner must lock 256x224 PNGs and 768x896 sheets |
| Stale 256x240/768x960 PNG claims replaced the distinct 256x240 video contract | Same dimension assertion plus manifest/proof distinction between PNG raster and AVI/video resolution |
| Wrong PNG dimension diagnostic omitted expected size | `CPU lifecycle original/native contact-sheet dimensions or equality contract is missing`; `Get-FileRecord` must report actual and expected dimensions |
| Pass with unavailable or nondeterministic video | `CPU lifecycle RequirePass/video and deterministic repeat-video contract is missing` |
| Missing/wrong contact-sheet dimensions or original-sheet equality | `CPU lifecycle original/native contact-sheet dimensions or equality contract is missing` |
| Incomplete live-common invariant | `CPU lifecycle running-clock/live-invariant gate regressed to pre-tip mode/screen detection` plus side/actor checks |
| Slot 10 stream offset serialized as unsigned data | `CPU lifecycle actor slot 10 incorrectly serializes stream or fixed-link data` |
| Candidate/switch/shot names inherit exact confidence | `CPU lifecycle candidate/switch/shot label confidence is not explicitly bounded` |
| Final progress or speed-mode evidence missing | `CPU lifecycle boot progress sentinel/watchdog is missing or uses buffered files` |
| Nonignored untracked proof inputs accepted under `-RequirePass` | `CPU lifecycle Git cleanliness/final-SHA/pending-metadata contract is missing` |

The standalone Lua parser executable was not present as a local command
(`lua`, `luac`, and `fceux` were not discoverable). This remained a local-tool
limitation; the formal FCEUX proof run passed separately.

## Reproduction command (formal run already executed; no rerun in this turn)

Do not run this automatically from static or CPU focused tests. The command is
shown for reproducibility; the formal run used privately supplied tool paths.

```powershell
.\tools\gameplay-lab\Run-GameplayCpuLifecycleProof.ps1 `
  -RomPath '<LOCAL_REV1_ROM.nes>' `
  -FceuxPath '<LOCAL_FCEUX_2.6.6.exe>' `
  -Build -RequirePass -RequireVideo
```

For a future `-RequirePass` rerun, Sol must first set
`$env:TECMO_CPU_LIFECYCLE_PERSONAL_SOL_INSPECTION='complete'`; the runner also
requires tracked and untracked nonignored worktree state to be clean and uses
the current Git HEAD as `final_sha`. `-RequirePass` also requires
`-RequireVideo`; a dirty no-`-RequirePass` rehearsal is explicitly `draft_pass`.

Expected runtime is bounded by two original-reference sessions of at most 4320
emulator frames each plus one warning-clean native build/focused test and 12
native render checkpoints rendered twice. Expected successful output is an
ignored `temp-videos/gameplay-lab/cpu-lifecycle/<timestamp>/` tree containing
source-pinned original traces, two 768x896 original contact sheets, 256x224
numbered FCEUX PNG reference frames, the separate 256x240 original AVI/video
contract, a fresh TGAI-1 pack identity, 640x480 native frames,
a validated 1920x1920 native contact sheet, a deterministic repeat set, and
two MP4 encodes with `-video_track_timescale 39375000` and exact NTSC cadence
`39375000/655171` validated by JSON ffprobe. Each probe must report matching
`r_frame_rate`/`avg_frame_rate`, `time_base=1/39375000`, dimensions 640x480,
and both `nb_frames`/`nb_read_frames` equal to 12. A missing ffmpeg/ffprobe is
draft-only; `-RequirePass` rejects it.

The proof is not a production parity claim: native frames come from the legacy
`gameplay-cpu-steering-frameN` continuity surface, whose scene still uses the
native harness/formation approximation. Integration remains R1-LIVE.

## Build/proof inventory

| Evidence | Status |
| --- | --- |
| Console/Win32 warning-clean build | passed by focused wrapper |
| TGAI-1 size/FNV identity | passed by focused wrapper |
| 680-record/opcode/handler/formation/route/play/shot goldens | passed by focused wrapper |
| Gameplay-lab static CPU proof checks | passed; new CPU surface and closed shot profiles both checked |
| Original FCEUX trace | formal clean `-RequirePass` passed; two deterministic runs and source-pinned evidence accepted |
| Native contiguous render/repeat/contact sheets | formal pass; original sheets 768x896, native sheet 1920x1920 |
| ffmpeg/ffprobe dual MP4 cadence/equal-hash check | formal pass; timescale, exact rate/time-base, dimensions, and both frame counts validated |
| Nonempty deterministic log inventory | formal pass; 100 artifacts and 36 nonempty runner-metadata logs |
| Personal Sol inspection | complete; independent QA accepted with only this docs-only P2 closure |

No ROM, decomp/ASM, FCEUX binary, capture, asset pack, PNG, video, or private
absolute path belongs in the commit.
