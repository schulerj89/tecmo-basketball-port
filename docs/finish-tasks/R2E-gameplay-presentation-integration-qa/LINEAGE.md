# Lineage and control record

## Identity and allocation

- master task: `019fc5d4-f360-78b3-b2a6-c8bae92df690`
- Sol task: `019fcc56-f41f-7601-8dae-e93ebef4ed4f`
- session: `S-SOL-R2E-GAMEPLAY-PRESENTATION-INTEGRATION-QA-001`
- task/claim/lane: `R2E-GAMEPLAY-PRESENTATION-INTEGRATION-QA` /
  `OWN-R2E-GAMEPLAY-PRESENTATION-INTEGRATION-QA` /
  `LANE-R2E-GAMEPLAY-PRESENTATION-INTEGRATION-QA`
- branch: `codex/r2e-gameplay-presentation-integration-qa-sol`
- worktree:
  `C:/Users/joshs/Projects/tecmo-basketball-port-r2e-gameplay-presentation-integration-qa-sol`

## Good-signed control

| Commit | Tree / sole parent | Durable decision |
| --- | --- | --- |
| `ab832e8a526ee464d9b63bbc1b9693bd1e2d9057` | tree `64db08ff8398f9cf69835580fb23e06f6d51552d`; parent `de02a2045fa9b785230fec6011c0f680409bdeba` | `control: accept presentation and reserve R2E integration` |
| `4edb11276b489353f29453dfd9dfab5b3e31ee4f` | tree `bcc0fbd5dc84e517077d728e1803e57588ed97ea`; parent `ab832e8a526ee464d9b63bbc1b9693bd1e2d9057` | `control: assign R2E presentation integration` |
| `ea57310a26bf16d6e32c514690d6165208c72fda` | tree `24724f0a387f8f1daf433ed4ddb1f7e80278246a`; parent `f1b12f8353ef5880a6f5efc572e5c4ba778f4b7e` | `control: record R2E takeover and correct stats template` |
| `20a4d4e1df04f718957348add849765b956ea315` | tree `24823c7a8a3c047a019364f6ada363f92a5cef34`; parent `e3a823063b1bbbce9cbe68335515d07072c22c3e` | `control: grant R2E runner scratch isolation` |

Sol personally verified every cited control. `git verify-commit` exited 0 and
reported a Good SSH signature for `jaystar524@gmail.com`, RSA fingerprint
`SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.

## Product and corrective ancestry

- reserved base, initial last-good, local main, `origin/main`, and live remote
  main:
  `ed060720a98b790f98591af363a490a0e0816018`, tree
  `32a741c530387ca331ac55349581cb67bf406a3b`
- candidate product commit:
  `4cb0c43bcd4c7ca111c996b3788e1bd00a734424`, tree
  `3bd5b4874eca46ff7ad771041e96946c3b08f233`, sole parent the base
- immutable accepted candidate and immutable staging:
  `d8d811918932c19bbe1741d2392ec1ad942ebd79`, tree
  `856ffbedfa7f3c1dc509310e96138e1aad140e5b`, sole parent the product
- runner corrective commit:
  `a4c1286351add0450b1820cd79876e04aa3a08f9`, tree
  `ef19835e58b87c3f02dec989f02d63db27a787a0`, sole parent the immutable
  accepted candidate

All four Git objects were personally checked for exact ancestry. The base,
product, candidate, and corrective commits are Good SSH signed by the same
authorized identity and key.

The prescribed branch-only action was exactly
`git merge --ff-only d8d811918932c19bbe1741d2392ec1ad942ebd79` from a clean
branch at the base. It produced no merge commit. The correction was then a
single ordinary descendant commit under exact Option-A authority. No rebase,
force, cherry-pick, main checkout/mutation, staging mutation, push, cleanup,
delete, or archive action occurred.

## Path and ownership ledger

The base-to-candidate ledger is exactly six regular text paths, 1,187
insertions, and 12 deletions:

- four files under `docs/finish-tasks/R2-gameplay-presentation/`;
- `src/tecmo_cli_render_gameplay_checkpoint.c`;
- `tools/Run-GameplayPresentationTests.ps1`.

The candidate-to-corrective ledger is exactly one modified regular text path,
`tools/Run-GameplayPresentationTests.ps1`, with 17 insertions and 4 deletions.
The base-to-corrective ledger remains the same six paths, with 1,200 insertions
and 12 deletions. There is no binary numstat entry, proprietary payload,
ownership escape, main-side collision, or diff-check failure.

The terminal report layer is restricted to exactly:

- `docs/finish-tasks/R2E-gameplay-presentation-integration-qa/README.md`
- `docs/finish-tasks/R2E-gameplay-presentation-integration-qa/COMMANDS.md`
- `docs/finish-tasks/R2E-gameplay-presentation-integration-qa/EVIDENCE.md`
- `docs/finish-tasks/R2E-gameplay-presentation-integration-qa/INDEPENDENT-QA.md`
- `docs/finish-tasks/R2E-gameplay-presentation-integration-qa/LINEAGE.md`

The report commit is the Good-SSH-signed descendant containing these five
files. Its exact SHA/tree are intentionally supplied by the terminal master
handoff because a commit cannot embed its own object ID.

## Independent task lineage

Exactly one top-level projectless Luna task was created after the post-ff
collision/registry gate and reused for the initial candidate, corrective tip,
and required terminal signed-tip reviews:

- task ID: `019fcc72-721f-7683-bdc2-26f69f0563be`
- title: `Tecmo R2E Gameplay Presentation Integration QA — Independent Luna Max`
- model/thinking: `gpt-5.6-luna` / `max`
- created: `2026-08-04T11:04:40Z`
- host: `local`
- directory:
  `C:/Users/joshs/Documents/Codex/2026-08-04/tecmo-r2e-gameplay-presentation-integration-independent-qa-luna`
- pin: `true`, retained through master acceptance

The create call succeeded once. A JSON-encoded return prevented the same
wrapper from extracting the ID for its pin call; Sol pinned the already-created
task immediately in the next call. No creation retry, second task, replacement,
or writable authority exists. The same task's first corrective-report send had
a formatting-delimiter transport rejection and succeeded on plain-text resend.
Neither fault changed repository or app state.

Initial Luna result: bounded product PASS with product severities all zero and
one QA-tooling P2 for fixed scratch/unconditional deletion. After exact
Option-A correction, the same task returned PASS with product and QA-tooling
P0/P1/P2/P3 all zero and explicitly closed the prior P2.

## Protected state and terminal protocol

At the corrective checkpoint:

- assigned branch: clean at `a4c1286351add0450b1820cd79876e04aa3a08f9`;
- local `main`: `ed060720a98b790f98591af363a490a0e0816018`;
- tracking `origin/main`: `ed060720a98b790f98591af363a490a0e0816018`;
- live `refs/heads/main`: `ed060720a98b790f98591af363a490a0e0816018`;
- immutable `codex/round-2e-gameplay-presentation-staging`:
  `d8d811918932c19bbe1741d2392ec1ad942ebd79`.

After the five reports are signed, the same Luna must verify the exact signed
tip, reports, proofs, refs, and clean state. Only the master may then fast-forward
main with a guarded non-force command. Sol stops before main or push and keeps
both Sol and Luna tasks pinned until Good-signed master acceptance.
