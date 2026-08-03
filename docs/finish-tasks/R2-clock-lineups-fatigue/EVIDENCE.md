# Evidence and behavior matrix

The matrix distinguishes source-grounded behavior from test-only composition.
It does not assert unported presentation, audio playback, substitution
ownership, or scene selection.

| Area | Classification | Sanitized source/evidence role | Implemented contract | Validation / limit |
|---|---|---|---|---|
| Clock decrement and expiry | `native_faithful` | Fixed-bank `$E58D-$E617`, `$E59B->$E823`, and `$E823-$E898` are the accepted clock/fixed-expiry path spans. | Native C event/state ordering is retained as a bounded semantic port, not a cycle-exact claim. | Exact fieldwise event vectors and unchanged-object alias vectors. |
| Period fixed wait/banner | `native_faithful` | Fixed-bank `$E7D0-$E822` and `$E80F-$E81E` are the accepted fixed-wait/banner transition spans. | At 0:01 with shot clock 10, entry emits late-clock SFX then game-clock expiry SFX; wait/completion edges emit no extra event where tested. | Entry plus 31-update completion vector; presentation claims remain bounded. |
| Possession reset | `native_faithful` | Reset evidence is fixed-bank `$E6ED/$E6FF` and `$E765-$E76F`. | Public reset accepts only a valid `LIVE` state. The established violation path enters `LIVE` before invoking reset. | Expiry, banner, halftime, final, complete, violation, foul, and free-throw non-LIVE states reject unchanged. |
| Period/final flow | `native_faithful` | Accepted Bank06 `$A05A-$A24F` and `$BC3C-$BCF9` flow spans; their role is period/final state flow, not an additional visual claim. | Supported regulation and tied-OT transitions retain native state/music/complete vectors. | Regulation durations `{2,3,4,8,12}` each cover final music; the retained tied-OT matrix maps them to `{1,1,2,3,5}` minute banners. |
| A-release dismissal | `native_faithful` | Bank03 A-release path `$EA14-$EA2F` and input path `$D2B9-$D2CE`. | Phase-frame saturation and controller-symmetric A-release dismissal are represented at the state boundary. | Fresh controller-0 halftime and controller-1 final paths; final emits exact `GAME_COMPLETE`. |
| Late clock | `native_faithful` | Existing clock event path and accepted fixed-bank spans above. | At the tested 0:01 boundary, late-clock SFX is the leading event when native code emits it. | Exact one-event fieldwise vector. |
| Shot expiry | `native_faithful` | Existing shot/game clock event path. | Non-exempt expiry emits expiry SFX then shot-expired detail `0`; exempt expiry emits expiry SFX then detail `1`. | Exact fieldwise vectors. |
| Simultaneous expiry | `native_faithful` | Existing ordering at 0:01 with shot clock 1. | Late-clock SFX, shot-clock expiry SFX, shot-clock-expired detail `0`, then game-clock expiry SFX. | Four-event ordered vector. |
| Update alias boundary | `native_faithful` | C object overlap is outside a useful ordinary typed-object contract; the guard is a conservative hardening boundary. | Reject only overlaps involving writable `state` or `events`; input/live-context overlap alone remains accepted. | State, input, context, event, and null-state unchanged/ordinary-contract tests. |
| TGFT evolution span | `exact_source_pinned` | Rev1 Bank02 `$B4E6-$B5C7` evolution and fixed caller `$ED2F-$ED3E`. | Canonical descriptors, bytes pointers, storage, scalars, payload hash, and dependency fingerprint validate together. | Metadata/pointer/object-overlap corruption fails closed. |
| TGFT cadence | `native_faithful` | Rev1 reload cadence is `6/4/1` and decremented in the same call. | Public post-step state rejects cadence counter `6` as unreachable; countdown `0` remains valid and wraps to `255`, while `255` remains valid. | Three mode counters, `0 -> 255`, `255 -> 254`, and cadence-6 transactional vectors. |
| TGFT active/bench/recovery | `native_faithful` | Rev1 active decay and the two team recovery paths. | Threshold behavior at/under 10, bench zero condition, +4 caps, both recovery countdown paths, and arbitrary unique caller active lists are preserved. | State/step vectors plus duplicate/out-of-range/malformed rejection. |
| TGFT live-tick coupling | `native_approximation_with_justification` | The pure kernel’s caller supplies difficulty and 2x5 active lists; production tick ownership is outside this lane. | Keep the kernel policy-free and transactional rather than inventing substitution/timing policy. | Focused vectors prove the kernel contract; production scene tick bridge is deferred. |
| TGFT parse/load | `native_faithful` | Native asset boundary and C alias safety. | Valid preloaded assets survive malformed/NULL replacement. Old-storage payload alias is staged safely; assets-object/storage overlaps reject before writes. | Sentinel object/storage tests and two successful loads. |
| TGFL source spans | `exact_source_pinned` | Rev1 Bank06 `$88B0-$88D9`, `$9621-$976E`, `$976F-$985C`, `$985D-$9918`. | All four canonical descriptors/bytes pointers, canonical internal pointers, lifecycle, storage hash/size, and dependency fingerprint are required. | `find_source` and both derive APIs fail closed on structured corruption. |
| TGFL base resolver | `exact_source_pinned` | Bank06 positioning/table relationship and shooter pose-preservation behavior. | `tecmo_gameplay_free_throw_lineup_derive` remains the pure base resolver; shooter/secondary slots are explicit and script fields undefined. | 2 orientations x 10 shooters x 9 distinct secondaries plus base golden actor vectors. |
| TGFL caller predicate tail | `exact_source_pinned` | Two contiguous predicate/effect tails are validated within Bank06 `$976F-$985C`; raw bytes are intentionally not reproduced here. | Shooter predicate nonzero applies raw `$0547=$36`, `$0551=$01`, `$057C=$04`; secondary predicate zero applies raw `$046E=$15`. No selection, aim, release, attempt, pose, ownership, or scene semantics are inferred. | Four `0/1` controls for every base placement (720 vectors) plus explicit `0xFF` nonzero vector. |
| TGFL parse/load | `native_faithful` | Native asset replacement boundary. | Failed replacement preserves a valid old object; struct aliases reject before writes. | Preloaded old-storage malformed replacement, NULL reload, assets/payload/core aliases. |
| Exact `$6023` starter staging | `exact_source_pinned` | Accepted exact working-state/RAM staging address `$6023`; it is not labeled as a CPU bank span and is not proof of stable-slot or substitution ownership. | No new production caller was added. | Recorded for later scene integration only. |
| Exact `$7B2E` starter staging | `exact_source_pinned` | Accepted exact working-state/RAM staging address `$7B2E`; stable slot mapping remains unproven. | No new production caller was added. | Recorded for later scene integration only. |
| Production stable slots/native selection | `native_approximation_with_justification` | Current scene supplies fixed actors `[0..4]/[5..9]`, roster indexes, active flags, launch starter arrays, and validation. | This worker does not reinterpret those stable slots or select replacements; preserving that current boundary avoids inventing ownership. | Requires the exact scene/game-flow rescope in `APPROXIMATIONS.md`. |
| Live substitution caller/eligibility/timing | `incomplete` | Pause/substitution labels/data do not prove a live caller, eligibility rule, or timing owner. | No live substitution integration or policy was invented. | Requires scene ownership and directly affected callers/tests. |
| Fixed-scene active lists | `incomplete` | Production scene’s current active lists/actor bindings are fixed at the scene boundary. | TGFT accepts caller-provided unique 2x5 lists but does not derive or replace them. | Production active-list ownership is deferred. |
| Builder transactions | `native_faithful` | Owned TGFL/TGFT import builders. | Payload/provenance stage and commit once; all ROM/payload/provenance pair overlaps reject without writes. TGFL enforces full canonical Rev1 SHA-256 inside the builder. | Sentinel failure tests and canonical focused runners. |
| Visual/audio output | `incomplete` | No visual/audio evidence was required for non-visible API/state changes. | No visual/audio claim is made. | Audio N/A; production visual proof is Sol-owned. |

The personally accepted read-only audit findings are durable here: the clock
audit established exact event ordering, tied-OT retention, symmetric score
screen requirements, and the writable-object-only alias boundary; the lineup
audit established that pause/substitution labels/data do not prove a live
caller and that only the bounded Bank06 predicate tail may be represented; the
fatigue audit established the cadence-6 post-step invariant, required the
`0 -> 255` byte-wrap vector, and required strict descriptor/storage and
transactional checks.

All runtime modules remain independent of the ROM/decomp/capture inputs. The
ROM is only an input to the focused source/build runners. Audio and visuals are
N/A for this matrix.
