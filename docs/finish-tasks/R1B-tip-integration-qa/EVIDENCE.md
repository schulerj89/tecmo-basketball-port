# Original evidence, native traceability, and proof

## Canonical source identity

The read-only source is `Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes`:

- size: `393232` bytes;
- SHA-256:
  `076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4`;
- full-ROM FNV1a32: `0650F5B0`;
- iNES identity and full ROM FNV/SHA are enforced by the TPTI-2 builder;
- no ROM byte, ASM/decompilation file, trace, capture, or save state is tracked
  by this branch.

The Sol independently re-extracted and hashed the source spans rather than
accepting the committed constants on trust:

| Source span | Bytes | FNV1a32 | FNV1a64 |
|---|---:|---|---|
| Bank04 `$86E1-$8817` | 311 | `F62D7C02` | `3940397C1774CFE2` |
| Bank05 `$8351-$839E` | 78 | `49B11D9A` | `A90A6C174BE0F15A` |
| Bank05 `$839F-$8402` | 100 | `39E37AD9` | `7591953DB1443C99` |
| Bank05 `$9824-$984F` | 44 | `40A15F8A` | `115114FC51CF97EA` |
| Bank05 `$985E-$988E` | 49 | `1A6CF134` | `3EE3400C4DD54BB4` |
| Bank05 `$985E-$986A` | 13 | `423816F1` | `032F8A7A4F4439D1` |
| Bank05 `$9C79-$9CC8` | 80 | `76854C2B` | `12DAA5698409484B` |
| Bank05 `$A214-$A2DE` | 203 | `ED30348D` | `0E50CF92E469E26D` |
| Bank05 `$98E1-$9A5F` | 383 | `0A2F945A` | `6CB90FF6825E2A5A` |
| Fixed `$E537-$E542` | 12 | `23EB540B` | `F47061919680B38B` |
| Fixed `$E56E` | 1 | `AC0BD104` | `AF64244C860266E4` |
| Fixed `$CD96-$CDAB` | 22 | `299318D0` | `FB37A5940B9FFA10` |

The accepted lifted-source spot check used Bank04 record
`C-0125_bank04_intro_capture_sequence_86E1_88A2.asm` and Bank05 records
`C-0081_bank05_lookup_tables_8351_839E.asm`,
`C-0082_bank05_state_machine_helpers_839F_8402.asm`,
`C-0110_bank05_dispatch_tables_9824_985A.asm`, and
`C-0111_bank05_large_state_and_trajectory_cluster_985B_BFA7.asm` for the
selected-actor, later commit, and claim addresses. These authoritative files
remained external read-only evidence; the stale/non-authoritative short
`$985B-$988E` derivative is not used as source of truth.

## ASM-to-native traceability

The table distinguishes ROM semantics from a native semantic bridge. Raw ASM
bytes are stored and validated as provenance; the native executable does not
execute those bytes.

| Original bank/address/span | Proven behavior | Exact native file/function consumer | Validating test/proof | Classification |
|---|---|---|---|---|
| Bank04 `$86E1-$8817`; RAM `$0761-$0764` | Clears the four capture bytes, derives the clocked error, samples the two B levels into the paired capture state, uses no-sample 12 / capped 11, then orders the pair. It does not prove native frame 0 as ROM timing. | `src/asset_pack/tecmo_asset_pack_gameplay_pretip.c::tecmo_asset_pack_build_gameplay_pretip` imports/hashes the span and `build_mechanics_block` stores the semantic contract. `src/tecmo_gameplay_pretip.c::validate_sources`, `validate_mechanics`, `parse`, and `assets_valid` bind it; `tip_error_for_sample`, `sample_tip_controlled`, and `tecmo_gameplay_pretip_update_controlled` implement the native fields transactionally. | `Run-GameplayPreTipTests.ps1` source-role/FNV/mutation gates; `tecmo_gameplay_pretip_self_test`; human frame-721 checkpoint and deterministic proof. | source-pinned; runtime-proven; native timing native-faithful/approximate |
| Bank05 `$8351-$839E` selected-actor dispatcher | Raw state table independently decodes selected actor `$1A->$8642` and `$22->$839F`. This proves `$8642` is a shared actor path and not slot-10 TIP logic. | Builder/source-map/validator retain the dispatcher. Runtime deliberately has no `$8642` TIP consumer; control enters native `tecmo_gameplay_pretip_update_controlled` through semantic team routing instead. | TPTI-2 `false-friend-8642` malformed vector; direct raw-pointer audit. | source-pinned; false-friend rejection |
| Bank05 `$839F-$8402` | Selected actor automatic gate reads ball height/current RNG-derived threshold and triggers only when ball high is strictly greater; equality returns because the original uses `CMP` then `BCS`. | `tip_automatic_threshold_met` is the strict comparator. `tecmo_gameplay_pretip_update_controlled` builds the threshold from parsed `tip_auto_threshold_base/mask/shift`; `tip_automatic_target_frame` supplies the bounded native 20/21/22 calibration. | C mechanics regression proves `0x42==0x42` false and `0x43>0x42` true; CPU/scene/proof checkpoints. | source-pinned and runtime-proven comparator; target cadence native-faithful/approximate |
| Bank05 `$9824-$984F`; opposing state `$13->$985E`; `$985E-$988E` and exact input subspan `$985E-$986A` | Opposing selected-actor path samples current B into the opposing capture byte, then waits/counts down toward the commit path. It proves held-level B semantics, not controller/team/receiver ownership or original CPU cadence. | Builder stores separate dispatcher, actor-path, and 13-byte overlapping TIP_INPUT evidence. Runtime `sample_tip_controlled` and `tecmo_gameplay_pretip_update_controlled` route two native controller/team inputs; parser/validators enforce the distinct subspan. | Exact-subspan FNV gate; missing/malformed/source-mutation tests; human, human-vs-CPU, and automatic scene regressions. | source-pinned input semantics; native routing native-faithful/approximate; ownership incomplete |
| Bank05 `$9C79-$9CC8`, commit entry `$9C7F` | Commits actor state `$0B`, pose/state `$02`, calls `$8D92`, and initializes actor motion/pose. | `build_mechanics_block` and `assets_valid` pin commit state `$0B`. `tip_commit_jumper` records sample, native velocity, and raw claim height; `scene_pretip_apply_jump_frame` / `scene_update_pretip_frame` present the jump. Native does not execute the raw actor workspace. | C self-test commit/state invariants; scene jump-presentation, descent/live, and abort/timing regressions; frames `661..725`. | source-pinned commit seam; runtime-proven transaction; motion bridge native-faithful/approximate |
| Bank05 slot 10 `$A214-$A2DE`; raw dispatch `$1A->$A25F`, `$1B->$A274`; claim span `$A274-$A2D5` | `$A214` forces slot 10 and dispatches. `$A25F` advances `$1A->$1B` while the ball is nonzero. `$A274` returns to `$1A` if zero, requires ball high at least `$3A`, compares jumper raw heights, returns on equality, selects the higher ready jumper, requires underflow-safe ball-minus-jumper less than `$3A`, touches opaque `$0380/$037F`, writes `$0478=$17` at `$A2D2`, and reaches the post-write seam `$A2D5`. | Source-map currently pins `$A214-$A2DE`, equality, `$A2D2`, and `$A2D5`; this report adds the independently decoded subdispatches without upgrading that committed string. `assets_valid` validates `$3A/$3A`, state `$17`, opaque selector addresses, and `$A2D5`. `tip_claim_ready`, `tip_try_resolve_claim`, `tecmo_gameplay_pretip_tip_winner`, and `tecmo_gameplay_pretip_claimant_jumper` implement the native semantic seam. | Direct pointer/ASM audit; `A2D1-non-hook` negative; claim-limit cache mutation; equality-deferral and winner-query C regressions; scene normal-home and CPU-to-live tests. | source-pinned; comparator/deferral runtime-proven; selector/receiver/team interpretation incomplete |
| Bank05 `$98E1-$9A5F` | Later/general actor collision and settlement family, including another B read. It is not the slot-10 TIP claim routine. | Imported as `LATER_GENERAL_COLLISION_SETTLEMENT` for provenance and rejected as TIP-claim proof. No native TIP claim function consumes it semantically. | Source-map `later_settlement` boundary; exact-role/span audit; false-friend review. | source-pinned false-friend rejection; TIP mapping intentionally absent |
| Fixed `$E537-$E542` | Exact bytes `AD FC 04 29 80 18 2A 2A 8D 58 07 AA`: `LDA $04FC; AND #$80; CLC; ROL; ROL; STA $0758; TAX`. `$E537` is entry and `$E542` is after the write, proving ordering only. | Builder/source-map records the 12-byte orientation-ordering span; `validate_sources` / `parse` preserve it. Native `scene_pretip_jumper_inward_facing` and `scene_pretip_apply_jump_frame` own explicit presentation facing, but do not claim `$E537` proves that ownership. | TPTI `E537-E542-ordering` negative/ordering gate; full-resolution contest anchor diagnostics; away-left LIVE checkpoint. | source-pinned ordering; native facing runtime-proven but native-faithful/approximate |
| Fixed `$E56E` one-byte anchor | Opcode byte `A9` is an executable anchor. Mapper-gated traces observe it recurring at the first running-loop entry; recurrence count is loop activity, not handoff count. | Builder/source-map/validator store exactly one source byte. No native runtime timing/transition function consumes `$E56E`; native handoff is scene-owned at update 721. | Focused `recurring-E56E-count` provenance gate; accepted original trace observations below. | source-pinned original-reference only; native frame-721 equivalence incomplete |
| Fixed `$CD96-$CDAB` | Exact bounded `$6A` RNG-mix seam. It does not prove the full downstream trajectory or TTDT/`$7C48` mapping. | `build_mechanics_block` records address/size; `tip_rng_mix` implements the deterministic native mixer; `tip_commit_jumper` uses only a bounded native bridge downstream. | Source FNV/mutation gate; state-valid RNG count/sequence regressions; deterministic double render. | source-pinned mix seam; runtime-proven mixer; downstream native-faithful/approximate |

## Limited `$E56E` original observation

The accepted mapper-gated R1 trace record was re-read and bounded as follows:

| Original scenario | `$E537/$E542` | First `$E56E` local/emulator frame | Total `$E56E` hits |
|---|---:|---:|---:|
| No input / no commit | `0/0` | none | `0` |
| Simultaneous input | `1/1` | `4007/4006` | `202` |
| Both automatic | observed handoff ordering | `4088/4087` | `211` |
| Human versus CPU | observed handoff ordering | `4034/4033` | `209` |

These are original-reference observations only. `first_e56e` is a bounded
running-loop observation, not proof that native update 721 is ROM-exact. Total
`e56e_count` is a loop-iteration count, not a possession-handoff count.

## TPTI-2 source-role and dependency audit

At merge `564d838...`, and unchanged after reconciliation `3aa7dfb...`,
`gameplay/pre-tip` is `7680` bytes with:

- SHA-256
  `C453848A33D6B29046D48ACDB44973D9A93234457C13F4150154F35DEA8F27FB`;
- FNV1a32 `28910BC1`;
- FNV1a64 `7EA1596E8DFAC0C1`;
- schema `tecmo.gameplay-pre-tip/TPTI-2`;
- 29 ordered source-role/span records;
- mechanics block magic/version `TPM2/2`; and
- full payload, reserved-byte, source containment, overlap, padding, and
  FNV32+64 validation.

The six dependencies must be read from the same pack and match exactly:

| Entry | Schema/size | Required FNV |
|---|---|---|
| `gameplay/core` | TGPL-1 / 23416 | `2047CCE0` |
| `menu/team-data` | TTDT-1 / 96372 | `812628F0` |
| `audio/music` | TMUS-1 / 36784 | `05C00ECB` |
| `arena/intro/warriors-transition` | TWAR-1 / 21024 | `3B22C51B` |
| `gameplay/jump-shots` | TGJS-2 / 2776 | `A66EE873` |
| `chr/all` | 262144 | FNV32 `F6F6E854`, FNV64 `96A64F53B240ABB4` |

`tecmo_gameplay_pretip_load` uses exact-size entry reads from one pack.
`parse` calls `validate_header`, `validate_sources`,
`validate_mechanics`, `validate_padding`, and `validate_dependency`
before making the asset available. Missing, wrong-sized, malformed, oversized,
cross-pack, stale TPTI-1, wrong-TGJS-version/fingerprint, mutated-ROM-source,
overlapping, out-of-bounds, and nonzero-padding inputs all fail closed.

The field-to-consumer boundary is precise:

- input/no-sample/max-error fields are parsed and revalidated; native sampling
  uses their identical compile-time semantic constants in
  `tip_error_for_sample`, `tip_sample_valid`, and
  `sample_tip_controlled`;
- automatic base/mask/shift fields are parsed, validated, and directly used by
  `tecmo_gameplay_pretip_update_controlled`;
- claim minimum/difference, commit states, selector addresses, and `$A2D5`
  are parser/validator-bound metadata; `tip_claim_ready` and
  `tip_try_resolve_claim` use the corresponding compiled semantic constants,
  while raw addresses/selectors are not executed or assigned invented meaning;
- TGJS-2 is both a strict same-pack dependency and the source of generic action
  poses consumed by `scene_pretip_apply_jump_frame`; and
- `append_gameplay_pretip_source_map_entry` emits the explicit role, span,
  dependency, proves/does-not-prove, winner, and runtime-input boundaries.

The committed source-map slot-10 string does not enumerate
`$1A->$A25F/$1B->$A274`. The traceability table records those independently
verified raw dispatches, but does not claim that the source-map contract itself
was changed.

## Deterministic native proof

Terminal reconciled proof root:
`build/proof/r1b-tip-integration-3aa7dfb523d6`.

| Artifact | SHA-256 | Acceptance use |
|---|---|---|
| `proof-manifest.json` | `1EAADF9972DB2751F5116A7F382389DEAFF0AB82232D67D215FC9AB9FE584493` | authoritative manifest pinned to `3aa7dfb...` |
| `proof-summary.txt` | `1AB2DDECF511EF1E1E17A84BE61C6D11B2902EC5EBF1DCAA3D44A11EFC2B5AB4` | concise manifest companion |
| `tecmo.assetpack` | `27D4CEB45D99F74C8C86C31B50FAEBC76AC71FFBFD92CA2A99478F01E1CA6B29` | canonical 86-entry same-pack input |
| `tipoff-stage-contact-sheet.png` | `4D29B5323D21B0C0CEACE359AFE6AB55E5EE1A7B54C783629769426D31B5EB95` | selected 1:1 full-resolution cells |
| `tipoff-left-edge-all-frames.png` | `4785DD027E8180A145517C824BC4AABEEA064EA39273450E316B3CC39BDB051A` | every-frame left edge/margin |
| `tipoff-right-edge-all-frames.png` | `7E8FF07AB0CF4D1FC3EDDCF582A8F2F82F359EAE03B5729593ADAF62E3B5BBB0` | every-frame right edge/margin |
| `away-left-facing.png` | `DDE21802E85DD14AC85F8792CBB9694C0833E5DC103A1C567891B1501F6FA783` | LIVE goal-facing checkpoint |
| `tipoff-sequence-0661-0725.mp4` | `4215BBF4733E71D2FFE8EC2D6C16DDF60AF187B7FE1585141320B34BEB8D4C20` | presentation only |
| focused `build/gameplay-pretip-tests/reference-comparison.png` | `23EA2D0980E6F37F059B65C1549EB9602433AE082D1E0A8861D6A59C68F6E479` | ignored 1024x2400 original/native comparison |

The current executable is `2079744` bytes with SHA-256
`E8061BD573CB275CF419C3AA9BE1AAC6F24E4AB758E7656681844F0897F67CEF`.
The manifest pins commit `3aa7dfb...`, schema
`tecmo.tipoff-realtime-proof/2`, 15 acceptance assertions, 29 TPTI-2 source
roles, and six exact dependencies. All 138 proof logs are nonempty; a bounded
warning/fatal/exception/failed/failure/`error:` scan, excluding diagnostic
`away-error`/`home-error` fields, found zero suspicious lines.
Every frame `661..725` is 640x480, is present in both passes, and matches its
independent rerender byte-for-byte. A direct relative-path SHA-256 comparison
against `build/proof/r1b-tip-integration-564d83835258` covered all 65 frames
and the contact sheet, both edge sheets, facing checkpoint, and MP4: 70 files,
mismatches `0`. Because that earlier proof already matched accepted R1, the
reconciled proof also matches accepted R1 byte-for-byte.

## Personal full-resolution visual review

The Sol personally reviewed first-pass frames `661`, `662`, `683`,
`687`, `696`, `720`, `721`, and `725`, both edge sheets, the contact
sheet, the away-left checkpoint, and the focused original/native comparison at
`564d838...`. After live-main reconciliation, the Sol again reviewed the
current-tip full-resolution contact sheet, contact/apex frame `687`, settled
frame `720`, handoff frame `721`, and away-left LIVE checkpoint.

- Both jumpers remain visible and inward-facing through the contest.
- Ball, players, center court, HUD, and active-view edges remain coherent.
- Crouch, rise, contact/apex, fall, landing, possession, and live-camera
  transition are visually continuous without clipping, corruption, a surprise
  snap, or margin leakage.
- Frame 721 is a deliberate native handoff/camera transition; it is visually
  accepted but not called a ROM-exact frame.
- LIVE restores Away-left goal-facing.
- The original comparison visibly differs in close-up/layout/timing details,
  reinforcing rather than erasing the declared native approximation.
- Reconciled-tip images are byte-identical to the personally accepted earlier
  proof; R2A/free-throw/fatigue integration introduced no visual difference.

Representative pixel scans showed nonblack active content bounded within
`x=64..575`, `y=32..479`, with zero transparent output pixels. Both host
margins are black across all 65 frames.

## Honest unresolved original behavior

The following remain incomplete and are not promoted by tests or visuals:

- complete original single-winner tie settlement;
- original selector, selected jumper, receiver, holder, and team ownership;
- TTDT attributes to raw `$7C48` velocity/trajectory mapping;
- complete original jump/ball trajectory and close-up presentation;
- ROM-exact native frame-721 timing; and
- a runtime semantic role for mapper-gated `$E56E`.
