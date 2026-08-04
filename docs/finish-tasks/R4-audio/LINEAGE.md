# R4 audio cue-routing proof lineage

## Signed authority

The active assignment is:

| Field | Exact value |
| --- | --- |
| Task | `R4-AUDIO` |
| Session | `S-SOL-R4-AUDIO-CUE-ROUTING-001` |
| Claim | `OWN-R4-AUDIO-PROOF-FIRST` |
| Lane | `LANE-R4-AUDIO-CUE-ROUTING` |
| Branch | `codex/r4-audio-cue-routing-sol` |
| Worktree | `C:/Users/joshs/Projects/tecmo-basketball-port-r4-audio-cue-routing-sol` |
| Base / expected parent / initial last-good | `f1b04193405d1c87f21e80ee51d3790499ea0cf8` |

The external control commit is
`709465dae0d648810e6990d1432c9cb133068253`, tree
`edb0aac71aed5401b4e4f9c99f64985f00efa1cc`, sole parent
`f1b04193405d1c87f21e80ee51d3790499ea0cf8`. Personal verification reported a
Good SSH signature for `jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. The assigned branch
remained at the expected parent for mutation; the control commit is not
silently merged into this task branch.

The exact five writable paths are the runner plus this four-document
directory. Every production, gameplay, season, importer, source-map, CMake,
build, and device path is read-only. Main, staging, origin, and push remain
master-only.

## Takeover and dependency checks

Before edits, the assigned worktree was clean on the exact branch and HEAD.
Root `AGENTS.md` and `PORTING.md` were read completely. The signed assignment
and active exclusive claim named the same five paths, with no overlap with the
concurrent R3 season claim.

The expected parent contains all four pushed task dependencies through their
accepted integration commits:

| Dependency | Integrated commit | Ancestor of expected parent |
| --- | --- | --- |
| R4 audio foundation | `bcacd5b6963f4db1a92c8db9b9770413505a0e98` | yes |
| R2 clocks/lineups/fatigue | `8a5b9928544a430efa34cbf98a248d6a8cbe7b14` | yes |
| R2 gameplay presentation | `bdc2fbb8f5b8497f4855b80e8834696220036aba` | yes |
| R4 frontend/intro/title | `edf16ca9059158452798dbe5667f5e64ef444e39` | yes |

The accepted isolated audio implementation lineage ends at
`e120c30ee882fe07b37496e2742ac83f1d16ff3a`; dedicated R4A integration and
reporting delivered it through `bcacd5b`. This task consumes that frozen
foundation. It does not revise it.

## Implementation/revision lineage

One persistent implementation/revision lineage produced the five proof-only
files and reports only to the authoritative R4-AUDIO Sol. It performed no
commit, merge, rebase, branch/worktree creation, build, private-ROM suite,
push, main update, or contact with another lane. Follow-up corrections must
return to this same lineage; no duplicate or replacement implementation worker
is justified.

The implementation decisions are:

- wrap the three accepted scripts rather than copying or replacing their
  checks;
- require `-DecompRoot`, making frontend route proof inseparable from the real
  native `--flow-test` execution;
- pin exact suite, route-source, foundation-doc, and R4A-report SHA-256 values;
- parse the newly regenerated foundation proof manifest rather than trusting a
  stale file;
- classify only five flow-executed frontend routes as proven;
- classify current gameplay/device callers as source-present-only;
- assert the absence of production DMC1/A8D6-long and DMC3/ABF5 queues;
- keep DMC IDs 0-2, effect 5, effect 6, and cycle/APU/device limitations
  explicit in both JSON and the short manifest.

## Pre-acceptance runner corrections

Both observed failures are harness defects and are retained rather than hidden:

1. Signed precursor `4c5b080a1f69f5bbbdba95913f3dbf9bd7ac7056`
   passed a string array to PowerShell splatting. Music bound the literal
   `-ProjectRoot` token as its value and failed before the suite ran. The same
   implementation lineage replaced this with named hashtable splatting.
2. Clean signed precursor
   `99a54943b4becc5e0dde0379624b6c7bb5f7cb8c` passed the build and all three
   frozen suites, then rejected the regenerated pack because it incorrectly
   treated R4A checkpoint `8916A549...C8141` as a timeless whole-pack golden.
   A separate GameplayAudio rerun passed and reproduced current expected-parent
   full-pack SHA-256
   `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`,
   while every audio payload and proof artifact identity remained exact.

Later accepted integration added non-audio pack content between the R4A
checkpoint and this task's expected parent. That is sufficient to make the
complete container identity integration-tip-specific; this lineage does not
attribute the delta to a narrower entry or byte. The runner now pins the
current full-pack identity and separately records the earlier accepted R4A
checkpoint as historical evidence.

The only executed validation in this implementation handoff was a parse-only
PowerShell syntax/static check of each local runner revision, which passed.
The authoritative Sol supplied the precursor runtime results above. A clean
full-wrapper PASS on the corrected committed candidate, current-HEAD artifact
hashes, personal Sol review, and exactly one independent terminal QA remain for
the authoritative Sol workflow.

## Handoff boundary

No commit SHA is fabricated here before the authoritative Sol creates and
signs the candidate. After terminal proof, the durable handoff must add the
exact signed candidate SHA, tree/parent, route-ledger SHA-256, ignored
route-manifest SHA-256, suite results, independent QA disposition, and clean
five-path audit.

Even after a passing proof, the deliverable remains proof-only. Promotion of a
source-present-only or unproven route requires concrete source/capture evidence
and separate signed writable authority for the affected production paths.
