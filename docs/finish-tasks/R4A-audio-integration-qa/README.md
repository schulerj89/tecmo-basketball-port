# Round 4A Audio Integration QA Acceptance

## Decision

**ACCEPTED for incremental main integration: isolated native audio foundation.**

This is the terminal report for `R4A-INTEGRATION-QA` / claim
`OWN-R4A-INTEGRATION-QA`. The accepted audio candidate is
`e120c30ee882fe07b37496e2742ac83f1d16ff3a`. After main intentionally advanced
with the independently accepted R3A lane, the integration branch reconciled
that candidate with `dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d` in signed
merge commit `0f06fd1102986c90fce65db251bf0c1806bd0c98`. This report is
the sole tracked change above that combined merge tip and must be committed as
a signed docs-only child after the content audit closes.

The integration finding counts are `P0=0`, `P1=0`, and `P2=0`. No product
defect was found. This acceptance lets the isolated audio foundation reach
main without waiting for frontend/menu work or broader cross-domain cue
routing. It does not close full `ACC-AUDIO`.

Only the master task owns main integration and push. This QA lane performed the
authorized branch-only reconciliation, but stops before any main ref update or
push.

## Session and control lineage

- Master task: `019fc5d4-f360-78b3-b2a6-c8bae92df690`.
- Integration-QA Sol task: `019fc8e1-282f-7322-a75c-ff994ba7f4cc`,
  `gpt-5.6-sol`, thinking `max`, pinned for the active audit.
- Master durable registration: control-plane checkpoint `f6b0a2a` recorded
  `S-SOL-R4A-INTEGRATION-QA-001`, transferred
  `OWN-R4A-INTEGRATION-QA`, and kept the task `in_progress`.
- Master reconciliation directive identified `dd096cb` as the intentional
  accepted R3A main advance and authorized merging it into this integration
  branch only, rerunning the combined gates, and handing back a non-force
  fast-forward result. It did not authorize this lane to update main.
- Sole independent auditor: task
  `019fc8e2-b17a-7f92-a9d6-9b7db43e74e3`, title
  `Tecmo R4A Audio Integration - Independent Audit Luna Max`,
  `gpt-5.6-luna`, thinking `max`, pinned, projectless, with null Git branch,
  worktree, base, and last-good fields.
- The Luna was created once after the collision/registry gate. There was no
  bad request, task fault, retry, replacement, discarded work, or worker
  lineage fork.
- Its initial independent read-only audit completed with
  `P0=0`, `P1=0`, `P2=0`. The same worker was retained for the final ignored
  proof and report inspection. Its next pass reported one control-plane P1 for
  the then-unreconciled `dd096cb` main movement and stale report rows, with no
  product defect; the authorized merge and report correction below resolve
  that finding. Its terminal combined-tip result is recorded below.

## Frozen Git candidate

The personal allocation takeover and frozen-candidate audit established:

| Check | Exact result |
|---|---|
| Worktree | `C:\Users\joshs\Projects\tecmo-basketball-port-r4a-audio-integration-qa-sol` |
| Branch | `codex/r4a-audio-integration-qa-sol` |
| Candidate HEAD | `e120c30ee882fe07b37496e2742ac83f1d16ff3a` |
| Frozen staging ref | `codex/round-4a-audio-foundation-staging` = `e120c30ee882fe07b37496e2742ac83f1d16ff3a` |
| Expected base / merge-base | `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb` |
| Candidate tree | `8e71afc688066ae9af7c7493e57041b3337d03df` |
| Staging tree | `8e71afc688066ae9af7c7493e57041b3337d03df` |
| `main` at allocation takeover | `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb` |
| `origin/main` at allocation takeover | `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb` |
| Tracked/untracked state | Clean before proof, after proof, and before this report |
| Patch hygiene | `git diff --check` clean |

The exact accepted linear single-parent writer lineage, in order, is:

1. `29611607babe31415ab063520d832631ab3c2e4c`
2. `51790b832eb4bb23db07ac7965d6c2b1da877a1e`
3. `c8eb88a8155b1d00471c964646a7f56f89cc6540`
4. `4ddb1bf3f5fda1e207e14ed443367afb5796a644`
5. `f9499e43503e69cbbe2f16774d73a4964a34adfc`
6. `ad82eb9ba34316b568b3ca33c80949657a973893`
7. `96471e42f79669379df0a573e8e82be037e559fb`
8. `9ac09214dfb167e6545d3e0422dbf2f2e7cfcad3`
9. `22199fb5a0b6a51641319ae5c61cc093e0a79444`
10. `e120c30ee882fe07b37496e2742ac83f1d16ff3a`

The product diff from the base contains exactly these 17 owned paths:

```text
docs/finish-tasks/R4-audio-foundation/EVIDENCE.md
docs/finish-tasks/R4-audio-foundation/IMPLEMENTATION.md
docs/finish-tasks/R4-audio-foundation/LINEAGE.md
docs/finish-tasks/R4-audio-foundation/MERGE.md
docs/finish-tasks/R4-audio-foundation/PROOF.md
docs/finish-tasks/R4-audio-foundation/README.md
docs/finish-tasks/R4-audio-foundation/TESTS.md
include/tecmo_audio_output.h
src/asset_pack/tecmo_asset_pack_gameplay_audio.c
src/asset_pack/tecmo_asset_pack_gameplay_audio.h
src/asset_pack/tecmo_asset_pack_music.c
src/tecmo_audio_output.c
src/tecmo_cli_audio.c
src/tecmo_frontend_audio.c
src/tecmo_gameplay_audio.c
src/tecmo_music.c
tools/Run-GameplayAudioTests.ps1
```

The reachable product commits add no tracked ROM, decompilation, save state,
asset pack, WAV, executable, image, or other generated/private proof artifact.
Main and origin/main were not changed by this lane.

## Combined-main reconciliation

During the first proof/report re-audit, `main` and `origin/main` were observed at
`dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d`. Read-only history identified that
commit as the accepted R3A season-data integration (`Document R3A season data
integration QA`), not an R4A action. Master confirmed that movement was
intentional and directed the branch-only reconciliation.

The exact collision gate compared both lineages from their common base
`6d8f9c7`:

```text
R4A candidate paths=17
R3A/main paths=9
exact path overlap=0
```

The resulting signed merge commit is:

```text
merge=0f06fd1102986c90fce65db251bf0c1806bd0c98
parent_1=e120c30ee882fe07b37496e2742ac83f1d16ff3a
parent_2=dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d
```

The merge completed without conflict. Its first-parent delta contains exactly
the nine accepted R3A paths; its second-parent delta contains the 17 accepted
R4A paths. Both `e120c30` and `dd096cb` are ancestors of the combined tip.
`main` and `origin/main` remained exactly `dd096cb` throughout the branch-only
merge and combined validation.

## Canonical evidence inputs

The suites used the local private canonical Rev1 iNES input and canonical
decompilation evidence root. No ROM/decomp bytes or generated pack/proof output
are committed.

- Revision: Rev1.
- ROM byte count: `393232`.
- ROM SHA-256:
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`.
- ROM visibility: local/private.

## Personal build and suites

The Sol ran these commands personally at the exact clean tracked/index
candidate `e120c30`, then reran the full set after reconciliation at combined
merge tip `0f06fd1`. `TECMO_SKIP_SHORTCUT=1`; `$RomPath` and `$DecompRoot`
resolved to the private canonical evidence inputs described above:

```powershell
.\build.ps1
.\tools\Run-MusicTests.ps1 -ProjectRoot (Get-Location).Path -RomPath $RomPath
.\tools\Run-FrontendAudioTests.ps1 -ProjectRoot (Get-Location).Path -RomPath $RomPath -DecompRoot $DecompRoot
.\tools\Run-GameplayAudioTests.ps1 -ProjectRoot (Get-Location).Path -RomPath $RomPath
```

Both builds exited zero, produced the native CLI and Win32 play executables,
and emitted no compiler or linker warning. The exact terminal combined-tip
suite dispositions were:

```text
MUSIC TEST PASS: TMUS-1 provenance parser sequencer synth cadence startup envelope null-sink frozen-fallback malformed missing oversized source-mutations
FRONTEND AUDIO TEST PASS: TFSX-1 exact-provenance parser stable-PCM title-stop-frame5 SFX10 frame1 track6 frame127 accepted-A-release SFX8 same-pack malformed missing oversized dependency frontend-source-mutations
GAMEPLAY AUDIO TEST PASS: TSFX-1 TDMC-1 provenance parser mixer override cadence music-gate mailbox DMC-independence DMC-continuity clear-all malformed missing oversized cross-pack source-mutations
```

These runs cover cross-module native audio regression behavior as well as the
isolated importers. The Music and Gameplay commands both exercise the portable
audio-output self-test.

## Source and negative-path audit

The personal source audit and the independent Luna audit agree on these
results:

| Contract | Audited result |
|---|---|
| Transactional waveOut initialization | One checkpoint is captured before the eight-buffer prefill. Any prepare/write failure restores borrowed player state, resets/unprepares/closes the device, and enters the documented silent fallback. |
| Transactional refill | State is checkpointed before each completed-buffer render. A rejected `waveOutWrite` restores that refill only, disables further service, and leaves earlier accepted device buffers for shutdown. Portable tests cover accepted and rejected music, gameplay, frontend/alias, and invalid-route cases. |
| Direct-render byte guards | Output, music, frontend, and gameplay render entry points reject `sample_count > SIZE_MAX / sizeof(int16_t)` before touching destination or player state, including a null sink. |
| Opening-once latch | `opening_queued` is set only after `tecmo_music_queue_track` succeeds. A missing-asset failure remains retryable. |
| Importer arithmetic | TMUS, TFSX, TSFX, and TDMC builders use checked 64-bit add/multiply, declared-PRG range checks, and the compatible `>=8` bank contract before forming source pointers. Offset-overflow, zero-bank, and seven-bank negatives pass. |
| Serialized postconditions | Strict Rev1 builds enforce exact sizes, payload FNVs, instruction counts, and voice counts after serialization and clear outputs on failure. |
| Same-pack routing | Loaders canonicalize pack paths. Canonical aliases are accepted; a byte-identical distinct container is rejected without replacing the prior route. Frontend/gameplay/music selections revalidate live shared-pack identity and detach invalid routes to music. |
| DMC safety/state | Parse, queue, and render layers enforce pool/clip relational bounds. The held DAC level persists through end, retrigger, and stop as declared. No reader phase or IRQ exactness is inferred. |
| Source mapping | Source-map roles, offsets, sizes, fingerprints, dependencies, priority masks, event conditions, unresolved IDs, and the track-8 queue source match the builder and script assertions. |
| Runtime boundary | Runtime initializes TMUS, then TFSX and gameplay from the selected shared pack; Win32 selects frontend/gameplay sources after output initialization. No excluded cross-domain call site was added or claimed. |
| Malformed pack negatives | SFX/TDMC payload byte mutations, cross-pack token mutation, missing entries, and oversized entries are rejected. |
| ROM mutation negatives | Gameplay directory/extension, DMC pool/trigger, A9C5/ABF5 sites, clock/countdown, gameplay event sites, pregame track-8 queue, music gate, and fixed engine mutations are rejected. |
| Cleanup | Music, frontend, and gameplay temporary test directories are absent after the runs; environment variables are restored in `finally`. The deterministic proof directory is intentionally retained only as ignored evidence. |

No product path was edited during integration QA.

## Deterministic proof and provenance

The gameplay suite regenerated fresh ignored proof first at clean candidate
`e120c30` and again at the clean tracked/index combined merge tip `0f06fd1`.
The terminal combined-tip root manifest is
`build/proof/r4-audio-foundation/proof-manifest.txt`; its SHA-256 is:

```text
5BC06D6E893BC9DA40215626CE714CDFF66F70FA58F192D5E488C490FC40A97D
```

The manifest records base `6d8f9c7`, head `0f06fd1`, the canonical private ROM
identity, an explicit validated pack source, 23 vectors, and 44.1 kHz mono
signed 16-bit little-endian PCM. The earlier e120 candidate checkpoint root
was `932AA1FE62BE3CC144C643A489353D2309434B81B5D8A4C6F1CB2CAE0F1B8FC5`.
All content identities remained exact across reconciliation:

| Artifact | SHA-256 |
|---|---|
| Generated shared pack | `8916A549E804AFF083B42989E898A92189A1226C192A644660B19812519C8141` |
| `audio-proof.wav` | `57573ABE791F4277AF6DCFC6E7AE22C7A7F319BC64554B0D7FDD8F16AFBC5D6B` |
| `audio-proof.events` | `3E8FB445B0774F847A529B2BC9670F81862F7C6C04B77AEFE7AB7D7D024674AA` |
| CLI `audio-proof.manifest` | `47EA2304FFF12C9348E821423E8E0806C9E00FA79DBE8344ED44E3C245B24298` |
| Waveform CSV | `76642CA7B52835301EEE0BA6185D50103C6DBC2A411D452A7FBFDBDCCFD5F4E2` |
| Waveform SVG | `6A6ED51A4BB1A77A76ACAA50DF1FA30D367AF5A273C9AB20D5C553EBD2A5A66E` |

Payload postconditions in the same proof are TMUS `36784` / `05C00ECB`,
TFSX `1792` / `985DC7ED`, TSFX `2824` / `968A5DE6`, and TDMC `2515` /
`AD70E6E8`.

The exact event header is:

```text
format=TECMAUDIOPROOF-1|sample_rate=44100|channels=1|bits=16|byte_order=little|records=fixed-fields
```

Every record has these 16 fields in this exact order:

```text
format, vector, name, start, count, queue, source, termination,
music_ticks, music_acc, music_playing, sfx_id, sfx_playing,
dmc_active, dmc_level, pcm_fnv
```

The exact vector order is:

1. `TMUS7_START`
2. `TMUS7_TAIL_END`
3. `TMUS5_LOOP`
4. `TMUS6_LOOP`
5. `TMUS8_END`
6. `TFSX8_DRY`
7. `TFSX10_DRY`
8. `TSFX3_DRY`
9. `TSFX5_DRY`
10. `TSFX6_DRY`
11. `TSFX11_DRY`
12. `TSFX12_DRY`
13. `TSFX13_DRY`
14. `TSFX14_DRY`
15. `TMUS5_TSFX3_OVERRIDE`
16. `TDMC0_CLIP`
17. `TDMC1_CLIP`
18. `TDMC2_CLIP`
19. `TDMC3_CLIP`
20. `TDMC4_CLIP`
21. `TDMC_POST_END_HOLD`
22. `TDMC_RETRIGGER`
23. `TDMC_STOP_HOLD`

Independent parsing, separate from the suite assertions, established:

- 23/23 records have the exact order and declared terminal state fields.
- Starts/counts are positive and contiguous from sample zero through exactly
  `4,331,648` samples.
- Every per-vector little-endian PCM FNV recomputes exactly: 23/23.
- The WAV is an exact 44-byte RIFF/WAVE PCM header followed by `8,663,296`
  data bytes; total file size is `8,663,340` bytes.
- The resulting duration is `98.223310658` seconds.
- Run 1 and run 2 WAV/events/CLI manifests are byte-identical.
- Run 1 and run 2 waveform CSV/SVG files are byte-identical.
- Whole-proof PCM statistics are minimum `-24079`, maximum `20927`, mean
  `-1571.905066`, RMS `4194.886485`, zero saturated samples, and maximum
  adjacent delta `24865`.

A separate projectless verifier repeated those checks against the combined-tip
proof and the committed expected-vector table, reporting:

```text
COMBINED PROOF AUDIT PASS
head=0f06fd1102986c90fce65db251bf0c1806bd0c98
root_manifest_sha256=5BC06D6E893BC9DA40215626CE714CDFF66F70FA58F192D5E488C490FC40A97D
vectors=23 fnv_recomputed=23 samples=4331648
wav_bytes=8663340 wav_data_bytes=8663296 duration=98.223310658
min=-24079 max=20927 mean=-1571.905066 rms=4194.886485 saturation=0 max_delta=24865
held_post_end=2100 held_stop=4987
```

## Objective waveform and spectrum inspection

The Sol inspected locally generated waveform overviews, a full spectrogram,
and per-vector detail spectra. These are objective file-derived observations,
not subjective listening:

- TMUS7 startup/tail, TMUS5/TMUS6 representative loops, and the terminating
  TMUS8 stinger contain time-varying harmonic energy with the declared vector
  boundaries and termination states.
- Frontend and gameplay dry cues have distinct envelopes, spectra, and PCM
  identities. The mixed override retains music energy while adding the cue's
  harmonic content.
- The five DMC clips show distinct broadband/low-frequency textures at the
  native renderer's declared semantic boundary.
- `TDMC_POST_END_HOLD` is a flat `2100` sample segment at held level 80;
  retrigger reintroduces changing energy and ends active at level 102;
  `TDMC_STOP_HOLD` is a flat `4987` sample segment after stop.
- No proof sample clips the signed 16-bit range.

Those candidate-inspection PNGs were temporary ignored aids and were removed
when the combined-tip suite regenerated the proof root. PNG retention is not
part of the committed proof contract. The terminal waveform CSV/SVG and all
underlying WAV/event identities remain exact and are the retained objective
evidence.

External human listening was separately approved for the clean opening,
tail/end, correct loops/stinger, separable cues, mixed override, and TDMC
held/retrigger/stop windows. That is external human signoff; neither the Sol
nor the Luna claims auditory perception.

## Provenance rejection and cleanup probes

The proof script's exact `Get-ProofRepositoryProvenance` implementation was
copied verbatim into a disposable projectless test repository. Normalized
function text comparison returned `verbatim_equal=True`; its normalized
SHA-256 was:

```text
CE6D92BDD202EF2D084DCFBE31398D8F643F39FF29E75B76FDF1444317D829B6
```

Behavioral probes passed with the exact diagnostics:

```text
PROVENANCE REJECTION PASS: dirty-index -> Proof repository has tracked or index dirt; commit the proof revision first.
PROVENANCE REJECTION PASS: wrong-base -> Proof-generation HEAD is not based exactly on the expected R4 base.
```

The dirty case used a staged tracked change. The wrong-base case used a clean
unrelated root commit with the same worktree tree, so repository dirt could not
mask the ancestry rejection. Both probes were outside the product worktree.
The assigned candidate remained clean at `e120c30`, and its retained root
manifest stayed at SHA-256 `932AA1FE...B8FC5` afterward. The later authorized
merge-tip regeneration changed only the path-free root provenance manifest
identity to `5BC06D6E...40A97D`; all content artifacts retained their golden
identities.

## Independent audit reconciliation

The sole Luna read all applicable `AGENTS.md`, `PORTING.md`, and the seven
committed R4 audio-foundation documents; audited the exact ten-commit lineage,
17-path footprint, implementation, scripts, docs, and integration call sites;
and returned this initial disposition:

```text
P0=0
P1=0
P2=0
```

There was no initial product finding to correct. The same pinned worker then
inspected the Sol-generated ignored candidate proof and draft report and
returned:

```text
P0=0
P1=1
P2=0
```

That sole P1 identified the advanced `dd096cb` refs and stale historical rows;
it explicitly found no R4 product defect. The master directive, zero-overlap
gate, signed `0f06fd1` merge, combined test/proof regeneration, and report
correction above resolve it. The same worker's terminal combined-tip re-audit
returned product/proof findings `P0=0`, `P1=0`, `P2=0`. Its only remaining
control-plane finding was this draft's pending-result placeholder and premature
claim that the not-yet-created report child was already signed. Those two
procedural statements are corrected here; no product, proof, or report-content
finding remains.

## Signature and fast-forward handoff

The branch-only merge commit has a verified SSH Git signature for
`jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. The terminal report commit
must be signed and independently verified with the same identity immediately
after the content audit closes. Its exact SHA and the final report-file
SHA-256 are supplied in the Sol-to-master terminal checkpoint; the commit
cannot truthfully embed its own object identity.

Because `dd096cb` is the second parent of `0f06fd1`, it is an ancestor of the
terminal report commit. Master can integrate without replay, rebase, force, or
merge commit creation:

```powershell
git switch main
git merge --ff-only <terminal-signed-report-sha>
git push origin main
```

Before that fast-forward, master should verify `main` and `origin/main` still
equal `dd096cb23a5fe7d755615fcaaadc0aa1d9b1509d`. This QA lane does not execute
those commands.

## Explicit deferred boundary

The following remain incomplete/deferred and are not accepted or implied by
this incremental gate:

- nonlinear or cycle-exact NES APU/PCM behavior;
- DMC reader bit phase, cycle phase, and IRQ timing;
- semantic resolution of DMC IDs 0, 1, and 2;
- anything beyond a neutral/unresolved effect 5;
- anything beyond bounded correlation for effect 6;
- excluded cross-domain cue-routing call sites;
- full product-wide `ACC-AUDIO` completion.

The accepted result is the exact deterministic native audio-foundation model
and evidence boundary documented here and under
`docs/finish-tasks/R4-audio-foundation/`, no more and no less.
