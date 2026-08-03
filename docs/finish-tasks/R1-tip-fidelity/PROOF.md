# R1 TIP Fidelity — proof manifest and Sol visual review

Status: source/ABI/pack/test review is accepted for the first TIP
implementation commit, but clean-commit formal visual proof and independent
QA are intentionally pending. Sol's latest broad scene wrapper run passed in
DRAFT mode and produced ignored output at
`C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna\build\live-proof-20260803T205847090Z`;
the independent Sol rerun produced
`C:\Users\joshs\Projects\tecmo-basketball-port-r1-tip-fidelity-luna\build\live-proof-20260803T204552541Z`.
`New-TipoffVisualProof.ps1` has not been run, and no generated artifacts are
committed.

## Required proof manifest

The eventual proof run must fail closed unless all of the following are present and internally consistent:

- complete contiguous full-resolution frame sequences for every requested checkpoint;
- exact video/frame counts and cadence;
- manifest SHA256 values for frames, contact sheets, and video;
- base/final binary SHA, ordinary TPTI-2 payload FNV32/FNV64, ROM identity, input route, resolution, and timestamps;
- mapper/state evidence for every claimed frame-721 handoff;
- clean tracked-tree status and clean command/log capture;
- original-reference comparison for unchanged and intentionally changed checkpoints;
- no missing frame, log, video, metadata, stale output, wrong dimension, cadence, or hash accepted.

The proof script now derives the home automatic logical sample frame rather than hardcoding it:

`$HomeSampleLogicalFrame = $ProofFirstFrame + $HomeAutomaticSampleFrame + 1`

With the accepted values (`661 + 21 + 1`), the runtime assertion requires `683`. The raw automatic sample remains frame `21` and its bounded capture error remains `11`; this distinction is part of the proof contract.

The broad wrapper DRAFT manifest self-validated with 254 inventory artifacts
and 255 files. It is diagnostic evidence only; a clean-tree formal proof and
Sol visual inspection remain required before any final PASS claim.

The worker's post-`43ab7a7` flow-fixture correction and the earlier
`f951098` live-proof fixture correction both preserve the real 721-update
handoff. The worker DRAFT contains `14` frames, `2` native videos, and a
`1920x1440` contact sheet; the independent Sol DRAFT has the same counts and
dimensions. Neither DRAFT is the formal proof.

The latest personal Sol gate snapshot also records warning-clean console+Win32
build, explicit console-flow and GUI/console production smoke, and passing
focused pre-tip and broad scene gates. Its DRAFT root is
`build/live-proof-20260803T205847090Z`.

## Expected changed visual checkpoints

The following eight checkpoints are the only expected pixel changes against the read-only TPTI-1 baseline. Each was independently rendered twice for base and current builds. Diffs are confined to the ball sprite bounds; players, court, HUD, and the accepted visible arc are unchanged.

| Checkpoint | Final SHA256 | Diff pixels | Bounds |
|---|---|---:|---|
| pretip680 | `A707A2C6D82DD2E2B2BAC3B5ABC7F790A95E13CC5BC825EEDEA330A5A9C1445C` | 136 | x314..331, y168..183 |
| pretip690 | `5272AFDC75E669C16B537D93D4CA96BCBC7DF283C835EE99D353CF31F5D6E43E` | 196 | x314..341, y188..203 |
| pretip696 | `4C4861E2992E0A431560B2F7AA0D7EFDDE47B6DEE96E772F931D93401EEDF86C` | 112 | x324..341, y202..215 |
| proof676 | `32F27C33BF01C9D07A8DAAA748FC05875F7DF3E6D82595E883B5DFE747335ED4` | 192 | x300..325, y160..175 |
| proof686 | `AD2AB7FCB8FBE1637C7EECA3D7C3FA14D1B57A35041778714EA471574E275D2E` | 176 | x298..325, y180..195 |
| proof687 | `33A3F3254F9928DBE81020FDF743E09BB9DC8B3E11B86370276F5C57FA989C2D` | 176 | x298..325, y182..197 |
| proof696 | `50B1264222C7797F2648414E70A13B232558D5BA03EEC378FD51CB1778288031` | 156 | x298..315, y200..215 |
| proof697 | `EAFD4442CDC8C7DB0CEB028BA5A3BA38B7A846AFCC7CC1A1C98DED7994626690` | 156 | x298..313, y202..217 |

Unchanged comparisons include pretip675, pretip712, proof668, proof712, and the card/toss/LIVE checkpoint set. Sol should visually inspect the eventual generated contact sheets and fill the review placeholders below.

## Sol visual placeholders

- [ ] Sol confirms the 60-update crouch/rise/apex/fall/land arc and both visible jumpers.
- [ ] Sol confirms no one-update ball-X snap, monotonic bounded travel, and the eight ball-only diffs above.
- [ ] Sol confirms frame-721 holder slot `0/5`, LIVE lineup/link/matchup state, clock/music/camera, and possession invariants.
- [ ] Sol confirms all generated frame/video/contact-sheet dimensions, counts, hashes, and metadata.
- [ ] Sol records final proof output paths and command logs here after the separately authorized proof run.
