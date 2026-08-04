# R4 audio route proof commands

The terminal proof requires the private canonical Rev1 ROM and a valid local
decompilation root. The decompilation root is required so the frozen
FrontendAudio runner executes its native `--flow-test`; omitting it is not an
accepted route-proof run.

No private path or input byte belongs in Git.

## Candidate preconditions

From the assigned worktree on the committed candidate:

```powershell
$ExpectedParent = 'f1b04193405d1c87f21e80ee51d3790499ea0cf8'
git status --short --branch
git rev-parse HEAD
git merge-base $ExpectedParent HEAD
git diff --check "$ExpectedParent..HEAD"
git diff --name-only "$ExpectedParent..HEAD"
```

Required results are a completely clean worktree/index, merge-base exactly
`f1b04193405d1c87f21e80ee51d3790499ea0cf8`, and exactly these five changed
paths:

```text
docs/finish-tasks/R4-audio/COMMANDS.md
docs/finish-tasks/R4-audio/EVIDENCE.md
docs/finish-tasks/R4-audio/LINEAGE.md
docs/finish-tasks/R4-audio/README.md
tools/Run-AudioRouteProofTests.ps1
```

## Terminal invocation

```powershell
$Rom = '<CANONICAL_PRIVATE_REV1_ROM>'
$DecompRoot = '<LOCAL_DECOMP_ROOT>'
.\tools\Run-AudioRouteProofTests.ps1 `
  -ProjectRoot (Get-Location).Path `
  -RomPath $Rom `
  -DecompRoot $DecompRoot `
  -Build
```

The wrapper passes `-Build` only to the frozen Music suite. That produces the
shared build once; FrontendAudio and GameplayAudio then run against the same
candidate without rebuilding. It requires each exact terminal line:

```text
MUSIC TEST PASS: TMUS-1 provenance parser sequencer synth cadence startup envelope null-sink frozen-fallback malformed missing oversized source-mutations
FRONTEND AUDIO TEST PASS: TFSX-1 exact-provenance parser stable-PCM title-stop-frame5 SFX10 frame1 track6 frame127 accepted-A-release SFX8 same-pack malformed missing oversized dependency frontend-source-mutations
GAMEPLAY AUDIO TEST PASS: TSFX-1 TDMC-1 provenance parser mixer override cadence music-gate mailbox DMC-independence DMC-continuity clear-all malformed missing oversized cross-pack source-mutations
```

The expected wrapper summary is:

```text
AUDIO ROUTE PROOF PASS: proven=5 source-present-only=13 unproven=7 ledger=<SHA256>
```

For this expected parent, the regenerated full shared pack must be:

```text
pack_sha256=27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29
```

The historical accepted R4A full-pack checkpoint `8916A549...C8141` is
retained in the ledger/manifest for provenance, but is deliberately not used
as the current full-pack assertion. Audio payload and proof artifact hashes
remain the accepted R4A values in [EVIDENCE.md](EVIDENCE.md).

The implementation/revision lineage intentionally did not run the build or
private-ROM suites before handoff. The authoritative Sol must record the exact
terminal command, candidate HEAD, ledger SHA-256, and result after the
candidate is committed and clean.

Two pre-acceptance runner faults are not terminal evidence: precursor
`4c5b080` passed literal string-array arguments to Music and failed before the
suite; clean signed `99a54943` then passed build and all three frozen suites but
rejected the reproducible current full-pack SHA because the wrapper still used
the historical R4A container checkpoint. The separately repeated GameplayAudio
suite reproduced `27D4CEB4...E1CA6B29` with unchanged audio identities. A full
wrapper PASS remains required after this correction.

## Ignored outputs

The existing GameplayAudio suite regenerates:

```text
build/proof/r4-audio-foundation/proof-manifest.txt
build/proof/r4-audio-foundation/run1/
build/proof/r4-audio-foundation/run2/
build/proof/r4-audio-foundation/waveform/
```

The wrapper then writes:

```text
build/proof/r4-audio-route/route-ledger.json
build/proof/r4-audio-route/proof-manifest.txt
```

The route manifest records candidate HEAD, expected parent, the route-ledger
SHA-256, regenerated foundation-manifest SHA-256, frozen suite hashes, counts,
and the explicit DMC/effect/cycle limitations. All generated output is ignored
and must remain untracked.

## Static script check

This parse-only check does not build or consume private evidence:

```powershell
$tokens = $null
$errors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile(
  (Resolve-Path '.\tools\Run-AudioRouteProofTests.ps1').Path,
  [ref]$tokens,
  [ref]$errors)
if ($errors.Count -ne 0) { $errors; throw 'PowerShell parse failed' }
```

After terminal proof and documentation revision, rerun `git diff --check` and
`git status --porcelain=v1 --untracked-files=all`. No build/proof artifact may
appear in the candidate diff.
