# R1 CPU lifecycle proof protocol

This is the reproduction contract for the accepted eleventh `DRAFT_PASS`
session and the formal clean proof that passed at proven code/doc HEAD
`8be7a9f9a11d43e68b090a98af122758885931fd`. Both sessions are recorded in
`EVIDENCE.md`; generated artifacts belong under ignored
`temp-videos/gameplay-lab/cpu-lifecycle/<timestamp>/` or an equivalent ignored
`build/proof` directory and must never be committed.

## Original-reference side

`Run-GameplayCpuLifecycleProof.ps1` locks the exact ROM SHA-256
`076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4` and FCEUX
SHA-256 `F89812F4E9506EF7090D9D0310D368ABD79BACA362B7BFC4A2E7E499754F2A1B`.
The Lua driver uses only complete two-port `joypad.set` tables. Its power-on
navigation schedule is the accepted deterministic/authentic controller schedule;
its row timing is not claimed as ASM/source-pinned input semantics:

| Frame interval | Port/button | Purpose |
| --- | --- | --- |
| 2600-4400, every 120 for 4 frames | P1 START | advance menu until validated setup |
| 2900-2906 | P1 A | select mode |
| 2941-2945; 2953-2957 | P1 DOWN | menu navigation |
| 2965-2971 | P1 A | confirm |
| 3087-3091 | P1 DOWN | team navigation |
| 3116-3123 | P1 A | confirm |
| 3201-3205; 3229-3235 | P1 RIGHT, then A | P1 team |
| 3321-3347 | P2 UP/A, then RIGHT x4/A | distinct P2 team |

After setup, the Lua applies P1 A at tip ages 30-34, A+B at 35-37, and B at
38-55. It waits until the exact stopped clock tuple (`minute=4`, `second=0`,
`shot_clock=0x18`) has been observed and then observes the canonical
running-clock/live setup before starting the post-live-delay capture.

The schedule is controller input only. No `memory.write*`, cheat, savestate,
ROM patch, or emulator state load is available to the script. Both pads are
written complete and neutral on every frame; the finalizer neutralizes both
pads before exit. The run first proves the MAN VS MAN setup, applies the
accepted deterministic/authentic tip schedule (A at ages 30-34, A+B at 35-37,
B at 38-55), and
does not define the live window until the exact stopped clock tuple has been
observed and the clock is running with canonical live invariants restored. It
is bounded at 4320 emulator frames, 180 live-window
frames, 8192 trace rows, 12 reference screenshots, 8 MiB tracked text, and
64 MiB whole-session output. It stops on startup/progress timeout, malformed
status, unknown mapper bank, unsupported context, or any cap breach.
The 4320-frame and 8192-row values are empirical deterministic proof-surface
capacity, not ASM/source-pinned gameplay semantics.

The CPU-specific map gates hooks to raw MMC3 Bank04/05/06 and records PC,
mapper select/registers, raw bank, A/X/Y, stream actor/offset, `$C7-$CB`
command bytes, handler address, actor state, and actor positions. The exact CPU
boundaries are `$8B90` fetch, `$8B9F` reader call, `$8BA2` copied-opcode
observation, and `$8BAE` indirect dispatch; `$8BB1/$8BC9` are static table
anchors and `$8BE1` is opcode 22's handler. Hook names are address labels only.
A record may include `intent_classification=deferred` even when the surrounding
routine name suggests a basketball action. A complete frame window fails closed
unless it contains positive fetch/opcode/dispatch/handler/advance observations,
aligned in-range actor stream offsets, and an exact fixed-link observation.
Every inventoried log is nonempty and carries sanitized runner label, command,
UTC start/end, exit result, and any raw tool output; silent successful streams
receive an explicit no-output record. The runner rejects a leftover
`progress.txt.tmp`, requires final `progress.txt` stage `finished`, a positive
sequence, and `speedmode_ok=true`.

The deterministic 120-frame live window emits exactly 12 numbered
full-resolution native-reference frames `reference-frame-0001.png` through
`reference-frame-0012.png` at the FCEUX `gui.savescreenshotas` 256x224
raster/crop, plus a separate 768x896 3x4 original-reference contact sheet for
each run, `trace.csv`,
`actors.csv`, `progress.txt`, and status metadata. The two original contact
sheet hashes must match.
The original AVI/video resolution remains a separate declared 256x240
contract; it does not redefine the emitted PNG raster or contact-sheet size.
Two independent sessions are compared by SHA-256 over trace/actor/frame-index
manifests. A mismatch fails closed. Actor slot 10 remains in the bounded actor
probe for `$046E`/position evidence but has no `$06CB` fixed-link entry and is
serialized as `NA`.

## Native side

The proof first runs the CPU focused wrapper, creates a fresh asset pack, and
checks the `gameplay/cpu-steering` entry for 7616 bytes/FNV1a32 `D6C4DB35`.
It then sets `TECMO_ASSETPACK` to that fresh pack and invokes the existing
production render surface for contiguous frames 25 through 36:

```text
tecmo_port.exe --root <project> --render-test-mode gameplay-cpu-steering-frame25 <png>
...
tecmo_port.exe --root <project> --render-test-mode gameplay-cpu-steering-frame36 <png>
```

Each numbered PNG must be 640x480. The same 12 modes are rendered a second time
and each SHA-256 must match. A native 3x4 contact sheet is validated at exactly
1920x1920. If ffmpeg and ffprobe are available, the already deterministic first
and repeat frame sets are each encoded at the exact NTSC cadence
`39375000/655171` with `-video_track_timescale 39375000`. Both videos are
probed as JSON with width, height, `r_frame_rate`, `avg_frame_rate`,
`time_base`, `nb_frames`, and `nb_read_frames`; both rate fields must equal
`39375000/655171`, `time_base` must equal `1/39375000`, both metadata counts
must be 12, dimensions must be 640x480, and the two video SHA-256 values must
match. A formal `-RequirePass` run requires `-RequireVideo`; it cannot
produce pass status with unavailable video. The original and native contact
sheets and videos remain separate non-parity evidence.

## Comparison limitation

The original trace and native render are not one-to-one lifecycle parity proof.
The original side proves source-pinned Rev1 execution evidence in a bounded live
window. The native side proves deterministic production rendering and current
asset identity. The production scene still uses the accepted native
harness/formation approximation and does not consume the isolated lifecycle
engine. Complete dynamic candidate/matchup state, inferred handler intent,
normal-flow integration, and shot outcomes remain outside this proof.

## Required manifest fields

The formal generated manifest carries base SHA
`6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`, proven final/code-doc HEAD
`8be7a9f9a11d43e68b090a98af122758885931fd`, pack/TGAI identity, ROM and FCEUX
fingerprints, exact input schedule, output resolutions, frame numbers and
timestamps, command lines, tool hashes/versions, artifact hashes, pass/fail
state, complete personal inspection, and the limitations above. The checked-in
`proof-manifest.template.json` remains template-only; its pending placeholders
are not generated proof metadata. The subsequent worker docs-closure commit
is docs-only and its terminal SHA is reported in the final handoff rather than
self-embedded.
