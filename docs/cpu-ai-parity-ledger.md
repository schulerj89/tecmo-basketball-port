# CPU/AI parity ledger

This is the denominator for claims about the native CPU/AI port. It deliberately
does not count all generated recomp functions: menus, audio, rendering, and
unrelated game modes would inflate that percentage without measuring gameplay
parity. Do not average the rows below into one headline percentage; each row has
a different behavioral weight.

Last audited checkpoint: `b52d1e6` (2026-08-24).

| Source denominator | Closed | Total | Coverage | Meaning |
| --- | ---: | ---: | ---: | --- |
| Aligned Bank06 playbook records parsed | 680 | 680 | 100% | Every imported five-byte record is structurally recognized. |
| Bank06 opcode handlers in the isolated executor | 24 | 24 | 100% | Pure handler semantics exist and pass the isolated source tests. This does not mean every production caller owns every input. |
| Imported formation starts | 46 | 46 | 100% | All pinned starts are present; upstream selection/admission is tracked separately below. |
| `$842E` shot-predicate input consumers | 9 | 9 | 100% | `$0588`, `$8545`, `$BA`, `$0478`, `$0798`, `$075F`, `$0760`, `$0533`, and `$006A` are wired with source ordering. |
| `$0588` bit-0 writer families | 4 | 6 | 67% | `$85F1`, `$8F37`, `$8EFB`, and `$9029` are bound. `$B235` and `$BAB3` remain. |
| `$A214` object-slot-10 dispatcher states with production bindings | 3 | 28 | 11% | State `$10->$B721`, `$17->$B783`, and pass state `$18->$B7B6` are bound. A low percentage here is important: the dispatcher is a high-leverage lifecycle owner. |
| Focused canonical-ROM mutation gates passing | 74 | 74 | 100% | Every currently declared authority span rejects mutation. This measures declared evidence, not undeclared behavior. |

## Remaining production-parity work packages

| ID | Source area | Status | Exact remaining boundary |
| --- | --- | --- | --- |
| AI-01 | `$842E` CPU shot admission | Partial | Bind `$B235` and `$BAB3` clearing lifetimes; then the predicate itself has no missing RAM input. |
| AI-02 | `$A214` slot-10 lifecycle | Partial | Audit and bind every reachable production state beyond the three current assignments; prove unreachable states explicitly. |
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

