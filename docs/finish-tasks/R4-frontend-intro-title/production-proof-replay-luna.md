# R4 frontend intro/title production replay proof — Luna revision 2

## Revision status

This document supersedes the earlier Luna proof document in fee87cfd831b82a75ffa8abccaabbfe8e9022115. The earlier run is retained below as superseded evidence only. Its finale hold, attract start, and title reset were obsolete for the integrated runtime boundary. The corrected integrated boundary is Sol commit a40dc3f9976d40444d91255f31a29959f5b23be3, with finale starting at global N=1508, attract at N=2509, and title reset at N=3151.

The source implementation remains the provisionally accepted commit 853e46ac3151cc80fdc432b9784e40e80f0edf1c, whose parent is the required base 6d8f9c7a99a7ce188f1a523247d3a9b9093860fb. This revision changes only this durable proof document on the registered Luna branch. All generated proof is ignored staging evidence.

## Scope

This worker owns exactly two tracked paths:

- src/tecmo_cli_render_scene_modes.c
- docs/finish-tasks/R4-frontend-intro-title/production-proof-replay-luna.md

The source change adds the strict CLI render-test mode intro-production-clean-frame<N>. It starts the real runtime at the normal TECMO_MODE_FIRST_SPRITE entry, advances the existing first-sprite production state machine with neutral input, and leaves the final draw to the generic tecmo_runtime_render path.

This is a render-test proof surface only. It does not change normal Win32 play, existing render modes, game/menu/audio/gameplay behavior, asset import, test scripts, headers, support/parser files, build files, or proprietary payloads.

## Non-goals and policy

- The mode creates no synthetic input. Every logical update receives an all-neutral TecmoInput, matching the production no-button state. Input proof is the existing --flow-test surface and is reported separately.
- The mode does not assign intro output-step, mode-frame counters, title state, attract state, or scene-local counters. tecmo_runtime_set_mode owns normal entry initialization, and tecmo_runtime_update owns every transition.
- The mode does not call a scene-specific renderer. The generic CLI handler calls tecmo_runtime_render once after the requested state has been reached.
- The runtime remains native and uses the semantic asset-pack path. ROM bytes, decompilation files, captures, traces, screenshots, videos, and proprietary payloads are not committed or runtime dependencies.
- The production replay video is intentionally silent and has no audio stream. Audio is outside this worker's ownership and the video is not input proof.
- Proof-v2 artifacts are ignored local evidence under the staging worktree. They are reproducible outputs, not tracked deliverables.

## Implementation and frame convention

The parser recognizes only the exact prefix intro-production-clean-frame. The shared decimal-suffix parser requires at least one decimal digit, base-10 digits only, no sign, no trailing characters, and no conversion overflow. This mode adds the safe inclusive bound N <= 4096. Missing, non-numeric, signed, trailing, overflow, and over-bound names are rejected without rendering an output PNG. N=4096 is intentionally an expected accept at the inclusive maximum.

For a fresh process, the convention is:

1. Runtime initialization completes.
2. TECMO_MODE_FIRST_SPRITE is entered through the normal runtime entry, yielding production title step 6, local mode frame 0, and global frame 0.
3. Exactly N calls to tecmo_runtime_update run with neutral production input.
4. The resulting state is rendered once by the generic tecmo_runtime_render caller.

Therefore N=0 renders frame 0 before any update. Handoff updates are included in the count: the update that reaches a production boundary resets the local mode frame to 0 for the new step, and that post-update state is rendered. The mode forces the debug overlay off and prints one concise sanitized state line:

    intro-production-state global=<global> step=<intro-step> local=<mode-frame> mode=<enum> attract=<0|1> title_armed=<0|1> title_confirming=<0|1> title_frame=<frame>

No direct intro-step/local-frame assignment or per-scene renderer call is used by this mode.

## Corrected production boundary map

The complete first neutral-input cycle has these inclusive rendered ranges:

| Global N range | Route | Intro step | Local frame | Boundary reached on next update |
| --- | --- | ---: | ---: | --- |
| 0..132 | title | 6 | 0..132 | 133 -> license |
| 133..409 | license | 7 | 0..276 | 410 -> arena |
| 410..949 | arena | 8 | 0..539 | 950 -> READY |
| 950..1007 | READY | 9 | 0..57 | 1008 -> WARRIORS |
| 1008..1221 | WARRIORS | 10 | 0..213 | 1222 -> CLIPPERS |
| 1222..1372 | CLIPPERS | 11 | 0..150 | 1373 -> BUCKS |
| 1373..1455 | BUCKS | 12 | 0..82 | 1456 -> PASS |
| 1456..1507 | PASS | 13 | 0..51 | 1508 -> finale |
| 1508..2508 | finale | 14 | 0..1000 | 2509 -> attract |
| 2509..3150 | attract | 15 | 0..641 | 3151 -> title reset |
| 3151 | title reset | 6 | 0 | normal cycle continues |

The corrected state samples include N=1507 step 13/local 51, N=1508 step 14/local 0, N=2508 step 14/local 1000, N=2509 step 15/local 0/attract 1, N=3150 step 15/local 641/attract 1, N=3151 step 6/local 0, and N=4096 step 8/local 535. The last sample is inside the next cycle's arena route and demonstrates that the safe maximum still follows the normal state machine.

### Finale internal map

The finale route is step 14 and has these production internals:

| Internal route | Frames | Global range | Local range | Selected state observations |
| --- | ---: | ---: | ---: | --- |
| opening-screen | 84 | 1508..1591 | 0..83 | local 0 phase=load black=1; local 83 phase=dispatch-wait black=1 |
| short-sprite-loop | 59 | 1592..1650 | 0..58 | local 0 phase=load; local 58 phase=dispatch-wait loop=7 anchor=142,240 |
| selector-transition | 52 | 1651..1702 | 0..51 | local 0 phase=load variant=1; local 51 phase=second-move palette=4 primary=1:48 |
| staged-group | 189 | 1703..1891 | 0..188 | local 0 phase=load; local 188 phase=dispatch-wait sprites=1 |
| title | 617 | 1892..2508 | 0..616 | local 0 phase=load; local 308 phase=title-write title_slots=21 primary=1:74 secondary=1:0; local 616 phase=dispatch-wait title_slots=44 primary=0:176 |

The internal terminator hold is local 1001. The normal production update hands off at N=2509, so the hold is not a rendered frame in this production-path sequence.

## Superseded evidence and harness notes

The fee87cfd document and its old ignored build/proof outputs are not used as evidence for this revision. They recorded the obsolete mapping finale 1508..2416, attract 2417..3058, and title reset 3059. The old generated proof therefore used finale hold 909, attract 2417, and reset 3059. Those claims are superseded by the integrated map above and are not silently carried forward.

The following intermediate harness faults are preserved for auditability. Each was detected and corrected before accepting proof-v2, with no source, state-map, or runtime-state impact:

- The first full-cycle state TSV harness wrote the literal characters backtick-t instead of actual tab separators because of a single-quoted PowerShell format string. Hex inspection found it; the file was repaired with a .NET replacement to actual tabs and then revalidated as 3152 contiguous data rows plus header.
- The first fresh-process determinism harness stopped before writing its manifest because PowerShell attempted int/string concatenation in the row expression. The harness was rerun with explicit joins and produced the accepted deterministic manifest.
- The first encoded-video boundary contact sheet treated sparse global-N filenames as a contiguous frame-%04d sequence, producing a repeated-first-frame sheet. It was discarded and regenerated by reindexing the extracted frames into a contiguous sequence. The corrected sheet is video/video-boundary-sheet-0000-3151.png.

- The strict parser manifest initially mislabeled every case expected_reject=True. Its accepted schema is now case, exit, output_exists, expected_outcome, passed, message: N=4096 is expected_outcome=accept with exit 0 and output_exists=True; missing, malformed, overflow, and N=4097 are expected_outcome=reject. The corrected manifest SHA-256 is 6FDCD5DE92F9258233C3090B77166D719BD4A24C21DB6F07DD0161E0E610AA9E.

## Verification inputs and commands

Verification used the detached staging worktree at the integrated boundary:

- Staging HEAD: a40dc3f9976d40444d91255f31a29959f5b23be3, detached.
- Staging tracked state: only the already-committed source patch was staged at src/tecmo_cli_render_scene_modes.c; no staging tracked file was edited or committed.
- Canonical Rev1 ROM SHA-256: 076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4.
- The canonical ROM, valid decomp root, and generated asset pack were local verification inputs only.

The build and canonical suite were run with the existing project commands:

    $env:TECMO_SKIP_SHORTCUT='1'
    .\build.ps1

The build passed with the existing /W4 compile setting in build.ps1 and produced the console/game executables without compiler errors.

    .\tools\Run-IntroSequenceTests.ps1 -ProjectRoot (Get-Location).Path -RomPath '<CANONICAL_REV1_ROM>' -Build

The canonical suite passed with failure_count=0 and skipped_count=1. The report was generated at 2026-08-03T17:57:28.5036974Z. The one skipped category is the suite's bounded reference category, not a failure of the production render mode. The same-tree generated asset pack was used by the proof:

    build/intro_sequence/tecmo_intro_sequence_test.assetpack
    bytes 1397729
    SHA-256 E97BC249441A11D2110D0F9E60A88CE9690C5EEBA384BA0BD023DFE17DB99886

The report is:

    build/intro_sequence_test_report.json
    bytes 271621
    SHA-256 B8C055E95EFB05C37955D937F542C4B7DDC26698C22013B1A69BF6E5665E25AD

The existing flow test was run separately with the valid local decomp root and the same asset pack:

    build/tecmo_port.exe --root '<VALID_DECOMP_ROOT>' --flow-test

It passed with:

    FLOW TEST PASS: menu play-intro title start-game-menu preseason season quit

This is the only input proof in this document. Its ignored log is build/proof-v2/manifests/flow-test-v2.log, 80 bytes, SHA-256 4143679BE4E14AE662BFB12F5A56BF2E2915D0D59BBDC302F098B4A5499512A4.

## Automated production-path results

### Full numbered sequence

The production sequence was rendered with intro-production-clean-frame<N> for every N from 0 through 3151 inclusive. It produced exactly 3152 numbered PNGs:

    build/proof-v2/frames/frame-0000.png
    ...
    build/proof-v2/frames/frame-3151.png

Every source PNG is 640x480 RGBA and 1229438 bytes. The full state map contains a header plus 3152 contiguous data rows. The render run log records count=3152. The endpoint N=3151 is the clean title-reset state and has the same black-frame SHA as N=0.

The principal full-cycle artifacts are:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| build/proof-v2/manifests/full-cycle-state-v2.tsv | 137701 | C789C426E4E866C2B58AE989E328D3188CF5A235A0896B12D502FC4AE26F6361 |
| build/proof-v2/manifests/full-cycle-png-sha256-v2.txt | 258464 | 54921E04B10ECDB7047B71E88F81859EEB16FAE4A21801DBBD21651A4C962845 |
| build/proof-v2/manifests/full-cycle-render-run-v2.log | 723 | BEF423C88D1A5A94FC2722C721D53A06B89F91A2AE532B134C1B71894DC1CCA2 |
| build/proof-v2/manifests/boundary-check-v2.log | 4032 | 6B163FD769CC9396ECCB37C1BAC1CE657BB4C8C0B0DE395C954D1CB0486956A7 |

The first and last source PNG hashes are:

| N | SHA-256 |
| ---: | --- |
| 0 | 2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A |
| 3151 | 2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A |

The full-cycle command shape, with private paths replaced, is:

    $env:TECMO_ASSETPACK='<LOCAL_ASSETPACK>'
    $root='<VALID_DECOMP_ROOT>'
    0..3151 | ForEach-Object {
        $n=$_
        $out=('build/proof-v2/frames/frame-{0:D4}.png' -f $n)
        .\build\tecmo_port.exe --root $root --render-test-mode ('intro-production-clean-frame' + $n) $out
    }

The actual proof harness also captured the sanitized state line from each process into full-cycle-state-v2.tsv.

The dedicated boundary log records exit 0 and sanitized state lines at every route handoff, finale internal endpoint, attract endpoint, reset, and N=4096. It confirms the corrected transitions 1508 step 14/local 0, 2509 step 15/local 0/attract 1, 3151 step 6/local 0, and N=4096 step 8/local 535.

### Strict parser and safe bound

The corrected strict parser manifest is:

    build/proof-v2/manifests/strict-parser-bound-v2.tsv
    bytes 1155
    SHA-256 6FDCD5DE92F9258233C3090B77166D719BD4A24C21DB6F07DD0161E0E610AA9E

Its schema is case, exit, output_exists, expected_outcome, passed, message. The accepted boundary is explicit:

- intro-production-clean-frame4096: expected_outcome=accept, exit 0, output_exists=True.
- intro-production-clean-frame4097: expected_outcome=reject.
- Missing suffix, alphabetic suffix, trailing characters, plus sign, negative sign, and 4294967296 overflow: expected_outcome=reject.
- Rejected cases produced no output PNG.

This test exercises the new mode only and does not weaken existing render modes.

### Fresh-process determinism

Two fresh process launches were compared at N=0, 410, 950, 1508, 1592, 1651, 1703, 1892, 2000, 2200, 2508, 2509, 2800, 3150, 3151, and 4096. Every PNG SHA-256 and sanitized state line matched exactly. The accepted manifest is:

    build/proof-v2/manifests/production-determinism-v2.tsv
    bytes 6000
    SHA-256 294ABD03828291F9B951AFA9CCA9EF6DDF94D1186344572ED7C468F29AFC0150

Representative hashes:

| N | SHA-256 |
| ---: | --- |
| 0 | 2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A |
| 410 | 1A14A50B0A32F3A76C57273898632B555BC7AEC14F970A6F3D02E0AB7934925D |
| 2000 | 7EE3256F0BD2473B281EE82AF2CA7C7F7605CA224E19E872EBBB2D47BF0C797A |
| 2200 | A82F2809FE789EC40073C42BF095F4505531A1D1BA02D3B950E0B29695BFFE9E |
| 2800 | FEADF78BAB704889A4047981F58B66684C26DFCE143839DFFA750308D8CFB153 |
| 3150 | 97A2811A207DAB9641C2134822D78FB60BC7BE34DEE9210FCA11DBC531DCCA17 |
| 4096 | DCEF437591EB89EAECF06A479A83F5B01114BEA846ADF69809824B35C4430381 |

### Per-scene parity overlaps

The production-path frames matched the existing per-scene clean render modes byte-for-byte in 25 overlap cases. The accepted manifest is:

    build/proof-v2/manifests/production-overlap-parity-v2.tsv
    bytes 7627
    SHA-256 93F3393C63FED86B01B14FF96BD8C24BDA2511600946B6FEC92B7E4DC1012D8B

The compared boundary pairs were:

- Arena: production 410/949 versus arena clean local 0/539.
- READY: production 950/1007 versus READY clean local 0/57.
- WARRIORS: production 1008/1221 versus WARRIORS clean local 0/213.
- CLIPPERS: production 1222/1372 versus CLIPPERS clean local 0/150.
- BUCKS: production 1373/1455 versus BUCKS clean local 0/82.
- PASS: production 1456/1507 versus PASS clean local 0/51.
- Finale opening: production 1508/1591 versus opening clean local 0/83.
- Finale short loop: production 1592/1650 versus short-loop clean local 0/58.
- Finale selector transition: production 1651/1702 versus reverse/selector clean local 0/51.
- Finale staged group: production 1703/1891 versus staged clean local 0/188.
- Finale title: production 1892/2200/2508 versus title clean local 0/308/616.
- Attract: production 2509/3150 versus title-attract clean local 0/641.

### Full-cycle video and encoded-domain comparison

The 3152 source PNGs were encoded twice as a deterministic, silent 60fps MP4. The encode used libx264 lossless qp=0, yuv444p, one encoding thread, bitexact flags, stripped metadata, and no audio. The command was:

    ffmpeg -framerate 60 -start_number 0 -i build/proof-v2/frames/frame-%04d.png -frames:v 3152 -c:v libx264 -preset veryslow -qp 0 -pix_fmt yuv444p -threads 1 -x264-params threads=1:lookahead-threads=1:sliced-threads=0 -fflags +bitexact -flags:v +bitexact -map_metadata -1 -map_chapters -1 -metadata title='Tecmo intro production replay' -metadata comment='Native deterministic 60fps proof' -an build/proof-v2/video/full-cycle-60fps-v2.mp4

The repeat encode used the same command and output the repeat filename. Both files are byte-identical:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| build/proof-v2/video/full-cycle-60fps-v2.mp4 | 1444648 | 9CBB34C4AD6F401103A37E06B2ABD874D86FC7F400D946DBEEF2742F2BAB8480 |
| build/proof-v2/video/full-cycle-60fps-v2-repeat.mp4 | 1444648 | 9CBB34C4AD6F401103A37E06B2ABD874D86FC7F400D946DBEEF2742F2BAB8480 |

ffprobe reports:

- 3152 video frames.
- 640x480.
- 60/1 frame rate and average frame rate.
- Duration 52.533333 seconds.
- H.264 High 4:4:4 Predictive, pixel format yuv444p.
- One video stream and no audio stream.

The probe record is:

    build/proof-v2/manifests/video-ffprobe-v2.json
    bytes 2676
    SHA-256 CC9D2916BAFAAF8C002BBE78D1832E5B08DC997521C18E3EF618CE46D1B44B02

The full source sequence was compared to the decoded encoded video in normalized yuv444p at time base 1/60. The PSNR output reported y=inf, u=inf, v=inf, average=inf, min=inf, max=inf. The consolidated proof-v2 manifest now stores the three component values as the strings inf, matching the other fields:

    build/proof-v2/manifests/video-psnr-v2.log
    bytes 1894
    SHA-256 37B2D0608C41DB7F2403D87503B3AB9D3F1834044B0BCCC6A9BFDB182B86B2AE

    build/proof-v2/manifests/video-psnr-stats-v2.log
    bytes 317330
    SHA-256 C7A17CF112D60505B02BB29ADEF56ADB996D3D78C1FFC0526DC09F2976775154

The comparison used the equivalent normalized filter:

    [0:v]format=yuv444p,settb=1/60[src];[1:v]format=yuv444p,settb=1/60[enc];[src][enc]psnr=stats_file=<PSNR_STATS>

The inf result is exact in the encoded YUV domain. It does not claim RGB PNG-container identity after decoding.

Thirty-three numbered boundary frames were extracted from the encoded video. Their container hashes are recorded separately from the source PNG hashes because extraction writes new PNG containers:

    build/proof-v2/manifests/video-extracted-v2.tsv
    bytes 2522
    SHA-256 02E3B3EB237E9EC5AB250A4EB57F3C42BB8C6AE9533C37280CFB49E86291A1BC

    build/proof-v2/manifests/video-extracted-sha256-v2.txt
    bytes 2904
    SHA-256 1B6930BB928D4FA52A0CFE656025FEFFA3698043010977A436BCF04AC8904C7B

Pixel comparison of sampled extracted frames found maximum channel delta 1 on non-black frames, with exact black/title-reset endpoints.
## Contact sheets and visual review

Numbered full-resolution range sheets cover all source frames in 64-frame ranges, with the final 3136..3151 sheet containing 16 frames. There are 50 numbered range sheets plus a boundary sheet. The range-sheet hash manifest is:

    build/proof-v2/manifests/contact-sheets-sha256-v2.txt
    bytes 4446
    SHA-256 35D21E86F33B9E4B1C8BFF6045DAC592F1578890D31AFEEA80D843EB3092F1D5

The selected PNG manifest contains 89 selected boundary, parity, determinism, and safe-bound PNGs. Sol independently checked 40 sampled PNG hashes from the selected manifest:

    build/proof-v2/manifests/selected-pngs-sha256-v2.txt
    bytes 7835
    SHA-256 1899C4973577BBD50FBB7D966E8B2EDA26876298A7C5834E08224DA33DAC8C84

The full-resolution boundary sheet is 1298x612:

    build/proof-v2/contact-sheets/boundary-sheet-0000-3151.png
    SHA-256 95A493726AC41D27F32D095537832117B6929A4FB2BC3E2A88280F19E18605A8

Its map is:

    build/proof-v2/manifests/boundary-contact-sheet-map-v2.tsv
    bytes 805
    SHA-256 C15CF23B700D14C1A1550EB0D69312958D6B52918FE83C5ACF5AADEDFB2B5597

The corrected encoded-video boundary sheet is also 1298x612:

    build/proof-v2/video/video-boundary-sheet-0000-3151.png
    SHA-256 0E327EF09A48EA2786F9869CC7BDB74D9BA409665381029272EBA207331180BC

The worker personally inspected full-resolution source frames at route boundaries and representative finale, attract, and reset points, including N=410, 1372, 1455, 1508, 1515, 1530, 1590, 1640, 1680, 1800, 1892, 2000, 2200, 2300, 2400, 2508, 2509, 2800, 3150, and 3151. The worker also inspected the corrected video boundary sheet and encoded-video extracts at N=410, 1372, 1891, 2200, and 3150. No crop, debug overlay, jump, or stale frame was observed.

### Sol personal review

Sol personally reviewed original-size range sheets for:

- 0000-0063
- 0384-0447
- 0896-0959
- 0960-1023
- 1024-1087
- 1216-1279
- 1280-1343
- 1408-1471
- 1472-1535
- 1536-1599
- 1664-1727
- 1728-1791
- 1792-1855
- 1856-1919
- 1920-1983
- 2112-2175
- 2176-2239
- 2432-2495
- 2496-2559
- 3136-3151

Sol also reviewed the boundary sheet and the corrected encoded-video boundary sheet, plus full-resolution global frames 1507, 1508, 1515, 1519, 1523, 1527, 1591, 1592, 1606, 1659, 1676, 1714, 1891, 2051, 2200, 2380, 2509, 2560, 2800, and 3150.

Sol's observations were continuous TECMO/license/arena pan; READY reveal; WARRIORS, CLIPPERS, BUCKS, and PASS motion; black handoffs; SUNS four fade stages; SPURS hoop loop; selector pass; BULLS staged reveal; finale title write/retract; attract logo/NBA figure; and clean reset. Sol observed no crop, overlay, jump, or stale frame.

Sol independently confirmed:

- 3152 numbered 640x480 RGBA PNGs.
- Contiguous state rows.
- 40 sampled PNG manifest hashes.
- Byte-identical MP4s with SHA-256 9CBB34C4AD6F401103A37E06B2ABD874D86FC7F400D946DBEEF2742F2BAB8480.
- ffprobe values of 3152 frames, 640x480, 60/1, 52.533333 seconds, H.264 High 4:4:4 Predictive, yuv444p, and no audio.
- Full-sequence PSNR y/u/v/average/min/max all inf.
- State boundaries finale 1508, attract 2509, title reset 3151.

This review is intentionally attributed to Sol. The production video is silent with no audio because audio is excluded from this ownership, and it must not be read as input proof.

## Consolidated manifest

The proof-v2 consolidated manifest records the integrated staging HEAD, canonical ROM hash, boundary map, all principal artifact paths, and the corrected PSNR strings. Its final corrected hash is:

    build/proof-v2/proof-v2-manifest.json
    bytes 24129
    SHA-256 D1AD002A59ADB4BFC7CD83614EF26620A10D642CBB1AEF47FEA1CFBE61906FCB

The manifest's video.psnr.y, video.psnr.u, and video.psnr.v values are JSON strings "inf", not null.

## Local reproduction paths and commands

Use placeholders for private local paths. Do not commit the ROM, decompilation tree, asset pack, generated PNGs, contact sheets, MP4s, or other proof payloads.

    $env:TECMO_ASSETPACK='<LOCAL_ASSETPACK>'
    .\build\tecmo_port.exe --root '<VALID_DECOMP_ROOT>' --render-test-mode intro-production-clean-frame0 build\proof-v2\intro-production-clean-frame0.png
    .\build\tecmo_port.exe --root '<VALID_DECOMP_ROOT>' --render-test-mode intro-production-clean-frame3151 build\proof-v2\intro-production-clean-frame3151.png
    .\build\tecmo_port.exe --root '<VALID_DECOMP_ROOT>' --flow-test

The ignored proof paths used in this revision are all below build/proof-v2, notably frames, contact-sheets, video, and manifests. The local staging path is not part of the tracked lineage.

## Limitations and deferred gaps

- This proof validates the native semantic asset-pack runtime and the production C state-machine path. It does not claim ROM-frame parity beyond the documented native boundaries.
- The neutral production video is not input proof. The separate --flow-test passed as reported above.
- Audio is intentionally excluded from this ownership; the encoded video is silent.
- The CLI mode maximum is deliberately 4096 updates, not an unbounded replay facility.
- Generated PNGs, videos, sheets, state maps, and local manifests are ignored evidence and must remain uncommitted.
- The canonical suite reported one skipped bounded reference category; it did not fail. No further gap was inferred from that skip.

## Lineage and merge handoff

- Required base and source parent: 6d8f9c7a99a7ce188f1a523247d3a9b9093860fb.
- Worker branch: codex/r4-frontend-intro-title-proof-luna.
- Source implementation commit: 853e46ac3151cc80fdc432b9784e40e80f0edf1c.
- Superseded prior proof-document commit: fee87cfd831b82a75ffa8abccaabbfe8e9022115. Do not cherry-pick it for the corrected proof.
- Corrected integrated staging commit: a40dc3f9976d40444d91255f31a29959f5b23be3.
- This documentation revision is committed after fee87cfd; the final documentation commit SHA is reported in the worker handoff.
- Only src/tecmo_cli_render_scene_modes.c and this document may be included in the worker lineage.

If Sol's branch already contains the source implementation, cherry-pick only the final documentation commit reported in the worker handoff. Otherwise the exact integration command is:

    git cherry-pick 853e46ac3151cc80fdc432b9784e40e80f0edf1c <FINAL_DOCUMENTATION_COMMIT_SHA>

Do not merge, rebase, force-reset, or push from the Luna worker worktree.
