# Lineage

## Task metadata

| Role | ID | Exact title | Model/creation metadata |
|---|---|---|---|
| Authoritative Sol | `019fc8ff-4ec4-7b20-86c6-9c9614f9194c` | `Tecmo R2 Clocks Lineups Fatigue Domain Orchestrator — Sol Max` | gpt-5.6-sol/max; completed v1 personal terminal QA/proof; v2 QA/proof and independent re-audit remain pending. |
| Writable implementation/revision Luna | `019fc912-a957-79f0-89a3-7e2e2d10db24` | `Tecmo R2 Clocks Lineups Fatigue Implementation — Luna Max` | gpt-5.6-luna/max; created_at `2026-08-03T19:21:11.000Z`; pinned=true during work; one successful creation, no fault, retry, or replacement. |

Writable registry details: branch
`codex/r2-clock-lineups-fatigue-luna`; worktree
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-luna`;
base/expected parent/initial HEAD/last-good
`222d75cfafa9153db1eb44492bf557f11b1a9091`. The authorized writable scope is
the complete path list in [SCOPE.md](SCOPE.md), including all listed headers,
owned gameplay/asset-pack sources, the two focused runners, and
`docs/finish-tasks/R2-clock-lineups-fatigue/**`; the eight product/runner paths
actually changed in implementation commit `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`
are distinct from that broader authorized scope.

## Accepted read-only audit lineage

These three reports were personally accepted as the evidence inputs for this
worker. Their findings are summarized again in `EVIDENCE.md` and
`APPROXIMATIONS.md`; the source reports were unpinned after those findings were
durably captured.

| ID | Exact title | Role and accepted finding |
|---|---|---|
| `019fc901-9608-76c1-ac13-4a5ef73f2e91` | `Tecmo R2 Clocks Periods Evidence Research — Luna Max` | Read-only clock/period evidence. Accepted exact event ordering, fixed-wait/final flow, supported duration matrices, phase behavior, and the boundary between proven state semantics and presentation claims. |
| `019fc901-d35e-7573-bf0d-36087859df58` | `Tecmo R2 Lineups Substitution Evidence Research — Luna Max` | Read-only TGFL/substitution evidence. Accepted the pure base resolver, the exact contiguous caller tail, explicit shooter/secondary slots, and the conclusion that pause/substitution labels/data do not prove a live substitution caller, eligibility rule, timing owner, or scene integration. |
| `019fc902-0bed-7d82-aa29-acaff4d9d04e` | `Tecmo R2 Fatigue Native Audit — Luna Max` | Read-only TGFT audit. Accepted reload cadence `6/4/1` with same-call decrement, unreachable public cadence `6`, exact active countdown `0 -> 255` wrap, threshold/cap/recovery asymmetry, and the required strict object/transactional hardening. |

No additional task or subagent was created by this writable worker.

## Independent QA lineage

| Field | Accepted historical value |
|---|---|
| Task | `019fc957-a425-70f3-83b9-1e63dfdba40e` — `Tecmo R2 Clocks Lineups Fatigue Independent QA — Luna Max` |
| Model/creation | `gpt-5.6-luna/max`; projectless/null-Git; created_at epoch `1785789391` = `2026-08-03T20:36:31Z` |
| Frozen candidate | `1536ae31e7016f6e9adbddb7868e2d40e51c1085` |
| Initial verdict | `FAIL` due to P2 only; explicitly no P0/P1 |
| Read-only integrity | Exact branch/HEAD/base/merge-base, four linear Good-signed commits, 18 allowed changed paths, clean/diff-check, and proof inventory/hash validation all passed. |
| Task state | Read-only QA remains pinned for independent re-audit. |

The v1 proof manifest SHA-256
`12DBA6C5D5D0C64C131DA35575CACBAAEA2D257198D57FC7C0B9D2DC11B043E1` is
bound to proof-source HEAD `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.
The frozen candidate `1536ae31e7016f6e9adbddb7868e2d40e51c1085` is its
docs-only descendant; no v2 proof was regenerated.

## Implementation revisions

1. Initial implementation added LIVE-only reset, ordered clock vectors,
   non-LIVE unchanged-state coverage, conservative writable-object alias
   guards, TGFT strict validation/transactions/vectors, and TGFL strict
   validation with a separate caller-policy API.
2. Fatigue evidence revision preserved countdown zero/255 behavior, added
   descriptor-pointer validation, staged builders, internal TGFL SHA-256, and
   staged runtime replacement.
3. Lineup review removed any interpretation of menu labels as live
   substitution semantics, kept the base resolver pure, and recorded the exact
   future scene rescope.
4. Clock review corrected the leading late-clock event, fixed-wait entry
   events, exempt detail `1`, tied-overtime matrix retention, fieldwise event
   equality, and the writable-only overlap set.
5. Score-screen review added isolated controller-0 halftime and controller-1
   final dismissals, exact final completion, and clean input setup.
6. TGFT review added NULL-assets fail-closed protection, state-vs-storage
   guards, valid staged reload, NULL failed replacement, corrupt-descriptor
   step rejection, and all builder pairwise overlap sentinels.
7. TGFL review changed opcode validation to contiguous predicate/effect tails,
   normalized public predicate names, added 720 policy vectors plus `0xFF`,
   parse alias guards, storage/object corruption coverage, and the exact runner
   message.
8. Independent-QA remediation staged all owned gameplay-state mutators and
   state/event outputs transactionally, added exact/partial alias and failure
   vectors, and hardened TGFT/TGFL destructor frees against bounded corrupt
   in-object storage without claiming arbitrary invalid-pointer detection.

## Commits

- Base/expected parent: `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- Implementation/tests: `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`.
- Documentation commit: `540ae0ba47ef44d6096781ffd0c276012e683221`.
- Documentation metadata correction: `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

All three commits report Good Git signatures for `jaystar524@gmail.com` with
RSA key fingerprint `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.
Sol fast-forwarded the signed lineage into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

## Sol QA and proof state

Sol's exact QA results, environment-precondition diagnosis, draft LIVE proof,
and deterministic proof manifest are recorded in `PROOF.md` and `TESTS.md`.
The task-specific proof root is
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-sol\build\r2-clock-lineups-fatigue-proof-20260803T203000Z`,
with manifest SHA-256
`12DBA6C5D5D0C64C131DA35575CACBAAEA2D257198D57FC7C0B9D2DC11B043E1`.
That manifest remains v1 proof-source evidence for HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`, not v2
proof for this remediation. Sol v2 QA/proof, independent re-audit, and the
terminal accepted SHA remain pending.

No merge, rebase, push, or main/staging mutation was performed in this worker
lane.
