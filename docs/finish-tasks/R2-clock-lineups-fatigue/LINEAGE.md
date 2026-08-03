# Lineage

## Task metadata

| Role | ID | Exact title | Model/creation metadata |
|---|---|---|---|
| Authoritative Sol | `019fc8ff-4ec4-7b20-86c6-9c9614f9194c` | `Tecmo R2 Clocks Lineups Fatigue Domain Orchestrator — Sol Max` | gpt-5.6-sol/max; completed v1 and v2 personal terminal QA/proof; independent re-audit PASS; QA remains pinned only until Sol completes the closure-doc consistency recheck. |
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

| Field | Accepted historical/closure value |
|---|---|
| Task | `019fc957-a425-70f3-83b9-1e63dfdba40e` — `Tecmo R2 Clocks Lineups Fatigue Independent QA — Luna Max` |
| Model/creation | `gpt-5.6-luna/max`; projectless/null-Git; created_at epoch `1785789391` = `2026-08-03T20:36:31Z` |
| Historical frozen candidate | `1536ae31e7016f6e9adbddb7868e2d40e51c1085` |
| Initial verdict | `FAIL` due to P2 only; explicitly no P0/P1 |
| Re-audit candidate/verdict | `1567f284ff48a2334fb6a9bd82d00aadf0cdb373`; `PASS`, no remaining actionable findings, no P0/P1 |
| Resolved findings | Public-state mutator alias/rollback P2; TGFT/TGFL destruction/replacement P2 within the bounded contract; P3 fixed-slot bridge, physical-frame/render-mode, and proof-source/candidate identity wording. |
| Integrity | Exact branch/HEAD/base/merge-base; clean status/diff-check; six linear first-parent commits, no merges; all six Good-signed by `jaystar524@gmail.com` with RSA fingerprint `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`; exactly 18 authorized changed paths. |
| Closure delta | `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be..1567f284ff48a2334fb6a9bd82d00aadf0cdb373` changed six task docs only. |
| Proof integrity | All 97 manifest artifacts matched paths, sizes, and hashes with no missing or extras; 81-frame, rerender, free-throw orientation, video/ffprobe/decoded-frame, no-audio, and draft LIVE manifest contracts matched the recorded Sol proof. |
| Auditor limits | The independent auditor did not rerun product tests or personally visually accept frames; Sol's v2 execution and visual acceptance remain authoritative. |
| Task state | QA remains pinned only until Sol performs a closure-doc consistency recheck and durably captures it. |

The v1 proof manifest SHA-256
`12DBA6C5D5D0C64C131DA35575CACBAAEA2D257198D57FC7C0B9D2DC11B043E1` is
bound to proof-source HEAD `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.
The frozen candidate `1536ae31e7016f6e9adbddb7868e2d40e51c1085` is its
docs-only descendant. The v2 proof manifest SHA-256
`1FA074FB90D87AF48A3FB78DB50E8B96A78C7F653EC9EFA76BF581B8FC0F51C3` is
bound to product/proof-source HEAD
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`; the re-audit candidate is its
docs-only descendant, not an artifact-integrity mismatch. The final accepted
SHA is the Good-signed closure descendant reported externally by its git
object/handoff, not a self-referential value in this record.

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
9. Sol personally completed v2 QA/proof at the remediation HEAD; all v2
   render artifacts matched v1 byte-for-byte. The independent re-audit passed
   with no remaining actionable findings and no P0/P1; QA remains pinned only
   until Sol completes the closure-doc consistency recheck.

## Commits

- Base/expected parent: `222d75cfafa9153db1eb44492bf557f11b1a9091`.
- Implementation/tests: `6c87dbed170c8ca2ba68e29671f7cfebf5adb60a`.
- Documentation commit: `540ae0ba47ef44d6096781ffd0c276012e683221`.
- Documentation metadata correction: `97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.
- Signed remediation: `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`.

All four commits report Good Git signatures for `jaystar524@gmail.com` with
RSA key fingerprint `SHA256:L/fBxE6/8x0E9W2UiVtyTLQ9mfI5AJDzdQYefIsj4fA`.
Sol fast-forwarded the original signed lineage into
`codex/r2-clock-lineups-fatigue-sol` at
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8` and accepted the remediation
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be` by ff-only integration.

## Sol QA and proof state

Sol's v1 exact QA results, environment-precondition diagnosis, draft LIVE
proof, and deterministic proof manifest are recorded in `PROOF.md` and
`TESTS.md`. The v1 task-specific proof root is
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-sol\build\r2-clock-lineups-fatigue-proof-20260803T203000Z`,
with manifest SHA-256
`12DBA6C5D5D0C64C131DA35575CACBAAEA2D257198D57FC7C0B9D2DC11B043E1`.
That manifest remains v1 proof-source evidence for HEAD
`97277cbecf685a9f8ac8e29dde1a6de61f0e2db8`.

Sol's v2 QA passed at exact product/proof-source HEAD
`bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`: warning-free build with
`TECMO_SKIP_SHORTCUT=1`, gameplay replay `7A204A525C79D21C`, asset-pack,
TGFL, TGFT, TGCP-2, TPNL-1, TGVR-1, full AssetPackTests, NativeFlowTests with
the exact validated pack, and GameplaySceneTests without `-RequirePass`.
The v2 LIVE draft is `build/live-proof-20260803T212356578Z` with 255 files
and manifest SHA-256
`4C522B29A0D82D5313F01D2C4436A46EF87635E4406DAD93527D18DD894A745E`.

The v2 deterministic proof root is
`C:\Users\joshs\Projects\tecmo-basketball-port-r2-clock-lineups-fatigue-sol\build\r2-clock-lineups-fatigue-proof-v2-20260803T212500Z`,
schema `tecmo.r2-clock-lineups-fatigue.proof/v2`, with manifest SHA-256
`1FA074FB90D87AF48A3FB78DB50E8B96A78C7F653EC9EFA76BF581B8FC0F51C3`,
98 files totaling `110,863,737` bytes, and `COMMANDS.txt` SHA-256
`F4A61AFB876319DB37ED3A25992A0DDF62FFE6131A93D7304E0DC7C6890B66E4`.
The 81 shot-clock frames, both free-throw PNGs, MP4, and all three sheets
are byte-identical to v1; selected frame and orientation rerenders also
matched byte-for-byte. Audio is N/A and no period/halftime/final render
ownership is claimed.

The v2 manifest is bound to `bf0ea4b40ac3d4cfd79a0391e4fad2acc30082be`.
The independent re-audit passed with no remaining actionable findings and no
P0/P1. Dynamic substitutions and production active-lineup ownership remain
`incomplete`; no exact visual/audio semantics claim is added. The QA task
remains pinned only until Sol completes and durably captures the closure-doc
consistency recheck. The final accepted SHA is the Good-signed closure
descendant reported externally by its git object/handoff, not a
self-referential value in this record.

No merge, rebase, push, or main/staging mutation was performed in this worker
lane.
