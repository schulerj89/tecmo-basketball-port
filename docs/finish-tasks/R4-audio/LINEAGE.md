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

Requested thread-backed Luna/max setup was attempted through `list_projects`,
`list_threads`, and `fork_thread`; each call timed out and was explicitly
terminated. No task or thread was created, so there was nothing to pin, and
this lineage does not claim a thread-backed Luna. To avoid a duplicate or
replacement, one persistent in-session same-model revision lineage was used.
At terminal, the service retry again timed out without creating a task. One
independent in-session reviewer, `/root/audio_terminal_qa`, completed the
terminal audit and returned PASS with P0/P1/P2/P3 all zero.

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

The implementation/revision lineage itself executed only parse-only
PowerShell syntax/static checks, which passed. The authoritative Sol supplied
the precursor runtime results above and performed the later terminal work.

## Authoritative terminal checkpoint

The authoritative Sol personally reviewed commit
`e68671f0087276b8374fee5144a716a7dfa57905`, tree
`573c0590da6e6b8ee63229d085e066d819322197`, sole parent
`99a54943b4becc5e0dde0379624b6c7bb5f7cb8c`. Personal verification reported a
Good SSH signature for `jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`. Its complete diff from the
expected parent remained exactly the assigned runner plus four R4-audio
documents.

From that clean signed checkpoint, the authoritative Sol ran the full wrapper
with the build and a valid canonical nested decompilation root. The CLI and
game executable built; Music passed; FrontendAudio passed with the real native
`--flow-test`; GameplayAudio passed. The wrapper reported exactly:

```text
AUDIO ROUTE PROOF PASS: proven=5 source-present-only=13 unproven=7 ledger=4A9664BD56CDEF6EE9B994F5834900367B13A6BD80A17A6F30A82FD281AD7DEB
```

The ignored route-manifest SHA-256 was
`6F3D0D40AB7946B3A1DA695808EC8615BC2881E9F113D1F427537FC52095DFD6`, and the
ignored foundation root-manifest SHA-256 was
`A681164E7C37864AEC6CD1DD88047DF2F374C308C7CAE1692B8B4E036A5E018E`.
The route ledger and both manifests bind `proof_generation_head` to `e68671f`.
The route ledger and route manifest additionally bind `expected_parent` to
`f1b04193405d1c87f21e80ee51d3790499ea0cf8`. The worktree was clean after
execution; only ignored artifacts existed.

This evidence revision was followed by the required clean final-HEAD rerun at
signed `8e58aa40f669e9f54155593b49b1e22638394111`. Its route ledger, route
manifest, and foundation manifest rehash to `C3D15BB6...BCEC`,
`D6ABBABD...11B9`, and `A87CF0D8...4F2D`, respectively, and each manifest
binds the exact final HEAD. Independent terminal reviewer
`/root/audio_terminal_qa` then accepted the exact five-path candidate with no
P0/P1/P2/P3 finding.

## Handoff boundary

The exact signed precursor `e68671f` checkpoint and final signed candidate
`8e58aa40f669e9f54155593b49b1e22638394111` are recorded separately. The
final candidate has tree `a4d8ffc22e2e379cc57d02f8c4ee6a8f9fec63f0`, sole parent
`e68671f0087276b8374fee5144a716a7dfa57905`, a clean five-path audit, clean
full-wrapper result, generated hashes, and an independent P0/P1/P2/P3-zero
terminal disposition. It is ready for current-main integration QA; it does not
authorize direct integration of a divergent branch or any broader audio claim.

Even after a passing proof, the deliverable remains proof-only. Promotion of a
source-present-only or unproven route requires concrete source/capture evidence
and separate signed writable authority for the affected production paths.
