# Focused test manifest

## Command

From the authorized worktree:

```powershell
.\tools\Run-GameplayDefenseContactTests.ps1 -RomPath `
  'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
```

The runner compiles a transient C harness plus the module under MSVC
`/std:c11 /W4 /WX`, executes it, and removes its GUID-named temporary
directory under the system temp path. Each translation unit receives an
explicit `/Fo` object path under that scratch directory, the linker output is
also under scratch, and compilation runs with scratch as its working
directory. The runner snapshots repository-root compiler/intermediate names
and rejects any newly created root artifact. No generated file is written to
the repository and no normal build file is changed.

The command is also run with the repository root as the caller and from a
separate caller directory. Both runs must leave these exact historical QA
artifact names absent from the repository root:

```text
defense_contact_harness.obj
tecmo_gameplay_defense_contact.obj
```

## Independent oracle coverage

The harness computes expected values independently and compares the returned
bytes; it does not establish correctness by calling the implementation twice.

### B06 metric and scan

- ordinary metric and deterministic repeat;
- wrapped 16-bit X, absolute depth, and 16-bit metric wrap (`0x007D` vector);
- stale no-match preservation and high-byte initialization;
- strict equal-tie rejection and high-index retention;
- descending `9..0` order, self skip, bit-`$10` gate, multiple improvements,
  output index mirroring, and high-index tie retention;
- a `0x0800` metric against `0x07FF` that rejects the old `<`/`|`
  precedence bug;
- a reversed-coordinate ordinary metric vector with the same result, providing
  orientation-neutral mirrored raw coverage;
- NULL, bad tag/address, invalid length/index, NULL table, input/result
  overlap, result/table overlap, and complete rollback;
- repeated identical scan output.

The B104 wrapper bytes are validated separately as raw boundary evidence. The
B081 API has no wrapper predicate field and exactly one scan pass.

### B05 `$9968`

- near-exhaustive signed-boundary coordinate matrix for X `-16..16` and depth
  `-16..16`, with independent subtraction/borrow oracle;
- accepted and rejected positive/negative boundaries;
- paired identical wrapped deltas with opposite borrow (`$FFFF` X and `$FF`
  depth);
- explicit high-byte rejection for `0x0100`, `0x0107`, and `0xFEFF`, plus
  mirrored `0xFFF8` pass / `0xFFF7` reject;
- output echo of derived deltas, borrow flags, raw gate, and invalid/NULL/
  alias rollback.

The positive/negative coordinate matrix and the B06 reversed-coordinate vector
are orientation-neutral mirrored raw coverage only; no human/CPU or other
semantic label is inferred.

The geometry API has no orientation, human/CPU, or semantic variant input;
those distinctions are intentionally outside this raw contract.

### B05 raw `$17` plan

- counter wrap at raw `$0754`;
- `$0588` mask/set, `$BA` bit set, both `$17` stores, and `$0743=0`;
- conditional nibble update true/false;
- both raw `$030C[$BE]` zero/nonzero plan branches; these are raw branch
  variants only, not human/CPU labels, and their semantic mapping remains
  incomplete;
- helper request `$C042`, X=`$07`, required route context, and no actual call;
- raw `$030C[$BE]` read-only condition and `$0754` read-only preservation;
- NULL, bad route/length/index/tag, NULL table, result/table overlap, input/
  result alias, and deterministic repeat.

### Dependency and provenance checks

- exact ROM size/SHA-256 gate;
- iNES magic, legacy/NES2 check, trainer-size calculation, PRG/CHR byte
  accounting, expected total, and bounded bank mapping;
- exact direct B06/B05 FNV-1a32 and SHA-256 span fingerprints;
- separate B06 enclosing-source mapping `$B081-$B365`, file offset `0x1B091`,
  length `741`, and SHA-256
  `AAA9670DA5942FA2614F925A266674893A352BB2DB3A8F4158F61C8AE891AE36`;
- negative rejection of invalid bank/CPU-window span mappings;
- raw B06 RTS/wrapper plus adjacent `$B109` and B05 following-opcode boundary bytes;
- read-only hash-before/hash-after checks for CPU, LIVE, TGSR, TPNL, and
  pretip boundaries;
- semantic boundary assertions against stable accepted public comments/docs;
- scratch compiler/linker isolation and no-new-repository-root-artifact
  postcondition;
- forbidden dependency scan for scene/audio/TPNL/TGSR names in the new module.

## Recorded result

The focused command passed after the review corrections:

```text
Focused isolated MSVC compile used /std:c11 /W4 /WX; no normal build integration.
R2 defense/contact tests passed: ROM size/SHA, iNES bank mapping, three direct raw-span FNV32/SHA fingerprints, enclosing B06 source-span SHA/length provenance, B081 scan oracle, B05 $9968 matrix, B05 raw-$17 plan, rollback, repeatability, and read-only CPU/LIVE/TIP/TPNL/TGSR boundary checks.
```

The harness itself also prints:

```text
R2 defense/contact raw tests passed: B081 metric+scan, B05 $9968 gate, B05 raw-$17 plan, transactional rollback, deterministic oracle
```

The corrective isolation reruns both exited `0` with the output above:

```powershell
# caller: C:\Users\joshs\Projects\tecmo-basketball-port-r2-defense-contact-luna
.\tools\Run-GameplayDefenseContactTests.ps1 -RomPath `
  'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'

# caller: C:\Users\joshs\Documents\Codex\2026-08-03\tecmo-r2-defense-contact-luna-worker
powershell.exe -NoProfile -File `
  'C:\Users\joshs\Projects\tecmo-basketball-port-r2-defense-contact-luna\tools\Run-GameplayDefenseContactTests.ps1' `
  -RomPath 'C:\Users\joshs\Projects\disassem\Tecmo NBA Basketball (USA) (NES-BK) (Rev 1).nes'
```

After each run, the exact repository-root names
`defense_contact_harness.obj` and `tecmo_gameplay_defense_contact.obj` were
absent. The enclosing B06 row is provenance-only and is not a fourth direct
implementation span.

This is raw/native test evidence only. It is not a scene, video, audio,
controller, or player-facing proof claim.
