# Independent terminal QA

Exactly one top-level projectless independent task was created and reused:

- ID: `019fcc72-721f-7683-bdc2-26f69f0563be`
- title: `Tecmo R2E Gameplay Presentation Integration QA — Independent Luna Max`
- model/thinking: `gpt-5.6-luna` / `max`
- created: `2026-08-04T11:04:40Z`
- directory:
  `C:/Users/joshs/Documents/Codex/2026-08-04/tecmo-r2e-gameplay-presentation-integration-independent-qa-luna`
- state: pinned and retained for the mandatory signed-tip follow-up

No second Luna, writable worker, subagent, retry, or replacement was created.
The task made no tracked/index/ref/main/staging/origin/product mutation.

## Initial candidate disposition

At immutable candidate `d8d811918932c19bbe1741d2392ec1ad942ebd79`,
Luna returned **PASS for the bounded product slice**:

- product `P0=0`, `P1=0`, `P2=0`, `P3=0`;
- one QA-tooling P2: the new runner used fixed
  `build/gameplay_presentation_test` scratch and unconditionally removed it
  recursively before/after a run.

Luna verified the Good-signed candidate, exact six-path text delta, parser and
fixture boundaries, 17-frame proof, two-pass determinism, six direct negative
replays, asset-pack/TGCS identity, scene evidence, affected native matrix, and
original-resolution visuals. It correctly kept the full scene proof
`DRAFT/PENDING`.

Because a pre-existing ignored directory could have been deleted or a
concurrent run raced, Luna did not invoke the unsafe wrapper. It replayed the
existing executable and proof directly instead. Sol classified the P2 as
actionable, stopped report authoring, and requested exact rescope.

## Option-A correction audit

Good-signed control `20a4d4e1df04f718957348add849765b956ea315`
authorized the one-runner correction. The same Luna task then audited exact
corrective tip `a4c1286351add0450b1820cd79876e04aa3a08f9`.

Final revised-tip disposition: **PASS**.

- product: `P0=0`, `P1=0`, `P2=0`, `P3=0`;
- QA tooling/integration: `P0=0`, `P1=0`, `P2=0`, `P3=0`;
- prior scratch-ownership P2: **CLOSED**.

Luna independently confirmed:

- exact SHA, tree, sole parent, subject, Good signature, one-path 17/4 diff,
  AST cleanliness, and `git diff --check`;
- per-invocation GUID scratch, build containment, pre-existence refusal,
  post-create ownership flag, exact-path cleanup, directory-type check, and no
  parser/proof semantic drift;
- no fixed or GUID scratch residue;
- focused manifest
  `DCFF7AD9670D0C630785401F8E083F10019E458BD23001E3575D124EF0F60E4C`,
  17x2 frames, 17 distinct hashes, six transactional negatives, and zero
  mismatch against accepted, fresh d8, or pass-two images;
- layup sheet
  `85995E1654354BFFF874AEA8510F91FD6E0AB1BCECD49C5138EBC116FB9B6A6C`
  and pack
  `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29`;
- scene manifest
  `E490A5C330D88D7B33C17E76C8CC855D5220C6C4B8A3E476909B3B125F7249B1`,
  254/254 inventory, matching sheets/videos, and honest DRAFT status;
- 23 affected/cross-domain executable checks, original-resolution layup/scene
  visuals, clean worktree, unique branch, and unchanged protected refs.

Luna's direct `--flow-test` remained unavailable because its isolated task had
no external decomp/baseline root. Sol's correctly rooted direct flow and
NativeFlow passed. The missing local original-reference manifest remains an
evidence limitation and not a defect.

## Registry and fault record

The create call succeeded once but returned JSON-encoded text; the wrapper's
same-script pin extraction therefore did not run. Sol recovered the returned
ID and pinned that same task immediately in the next call. This is a pin-wrapper
transport fault, not a task retry or replacement. The same pin was later
reaffirmed idempotently.

During the revised audit, Luna's first attempt to send its final report rejected
a formatting delimiter. It resent the same report in plain text from the same
lineage. No app or repository state changed.

## Signed-tip protocol

After these five reports are Good-SSH-signed, the same pinned task must review
the exact terminal SHA/tree/parent/signature, candidate-to-tip path ledger,
runner/report contents, proof hashes, protected refs, and cleanliness. The
signed-tip verdict belongs in the guarded master-only handoff; this pre-commit
report does not pre-approve a future object ID.
