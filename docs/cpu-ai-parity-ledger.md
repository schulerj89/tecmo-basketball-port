# CPU/AI parity ledger

This is the denominator for claims about the native CPU/AI port. It deliberately
does not count all generated recomp functions: menus, audio, rendering, and
unrelated game modes would inflate that percentage without measuring gameplay
parity. Do not average the rows below into one headline percentage; each row has
a different behavioral weight.

Last audited behavior checkpoint: `a48304d` (2026-08-24).

| Source denominator | Closed | Total | Coverage | Meaning |
| --- | ---: | ---: | ---: | --- |
| Aligned Bank06 playbook records parsed | 680 | 680 | 100% | Every imported five-byte record is structurally recognized. |
| Bank06 opcode handlers in the isolated executor | 24 | 24 | 100% | Pure handler semantics exist and pass the isolated source tests. This does not mean every production caller owns every input. |
| Imported formation starts | 46 | 46 | 100% | All pinned starts are present; upstream selection/admission is tracked separately below. |
| `$842E` shot-predicate input consumers | 9 | 9 | 100% | `$0588`, `$8545`, `$BA`, `$0478`, `$0798`, `$075F`, `$0760`, `$0533`, and `$006A` are wired with source ordering. |
| `$0588` bit-0 writer families | 6 | 6 | 100% | All six bit-changing stores are translated: `$85F1`, `$8F37`, `$8EFB`, `$9029`, terminal pass writer `$B235`, and direct-carom BA65 writer `$BAB3`. The upstream direct-carom lifecycle remains tracked by AI-02. |
| `$A214` pointer-table entries imported and verified | 28 | 28 | 100% | Every state resolves to its exact Bank05 handler. This is structural coverage, not semantic runtime coverage. |
| `$A214` states routed through the generic dispatch API | 3 | 28 | 11% | States `$10`, `$17`, and `$18` use the generic resolver. This is an integration-style metric, not a CPU/AI completion percentage: existing native subsystems implement other state semantics directly (including pre-tip `$1A/$1B`) and must be credited by the semantic audit below. |
| Focused canonical-ROM mutation gates passing | 74 | 74 | 100% | Every currently declared authority span rejects mutation. This measures declared evidence, not undeclared behavior. |

## Remaining production-parity work packages

| ID | Source area | Status | Exact remaining boundary |
| --- | --- | --- | --- |
| AI-01 | `$842E` CPU shot admission | Closed | All nine predicate inputs and all six bit-0 writers are source-ordered and bound. |
| AI-02 | `$A214` slot-10 lifecycle | Partial | Classify all 28 handlers by gameplay role, credit already translated semantic owners, then bind only reachable missing states or prove them unreachable. Do not equate generic-resolver call count with AI coverage. |
| AI-03 | `$96B6` route admission | Partial | Replace raw `$030C/$030D` extra-adjust assumptions with the exact controller/side owner. |
| AI-04 | `$8FAD` made-score handoff | Partial | Bind the admission mismatch and state-`$0B` resolver instead of entering only after a typed caller decision. |
| AI-05 | Play/primary/defender selection | Partial | Close remaining selected-primary gates, raw workspaces, and the selected-defender state transition currently protected by fail-closed adapters. |
| AI-06 | Deferred opcode caller inputs | Partial | Remove remaining `missing-*` production deferrals by binding their actual Bank05/fixed-bank producers or proving them unreachable from all 46 starts. |
| AI-07 | Playback and integration proof | Open | Run normal CPU-vs-CPU gameplay through the desktop executable, capture video, compare event/state traces, and resolve every divergence before final parity. |

## Claim rules

- “Handler-complete” requires 24/24 isolated handlers; that condition is met.
- “Production CPU/AI complete” requires every AI-01 through AI-07 row closed,
  no CPU-related `missing-*` production defer reason, and no native
  approximation classification on an AI decision or movement path.
- “100% wired” additionally requires the final desktop build, fresh asset pack,
  deterministic tests, ROM mutation suite, CPU-vs-CPU video/trace evidence,
  and a clean pushed `main` checkpoint.
