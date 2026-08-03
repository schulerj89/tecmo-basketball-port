# R4 frontend intro/title production replay proof

## Scope

This worker owns exactly two paths:

- `src/tecmo_cli_render_scene_modes.c`
- `docs/finish-tasks/R4-frontend-intro-title/production-proof-replay-luna.md`

The source change adds the strict CLI render-test mode
`intro-production-clean-frame<N>`. It starts the real runtime at the normal
`TECMO_MODE_FIRST_SPRITE` entry, advances the existing first-sprite production
state machine with neutral input, and leaves the final draw to the generic
`tecmo_runtime_render` path.

This is a render-test proof surface only. It does not change normal Win32 play,
the existing render modes, game/menu/audio/gameplay behavior, asset import,
test scripts, or build files.

## Non-goals and policy

- No synthetic input is generated; each logical update receives an all-neutral
  `TecmoInput`, matching the production no-button state. Input proof remains
  the existing `--flow-test` surface.
- The mode does not assign `intro_output_step`, `mode_frame_counter`, title
  state, or any per-scene state. `tecmo_runtime_set_mode` owns the normal
  title-step/local-frame initialization, and `tecmo_runtime_update` owns every
  transition.
- The mode does not call a scene-specific renderer. It returns control to the
  generic CLI, which calls `tecmo_runtime_render` once for the selected state.
- ROM bytes, decompilation files, captures, traces, screenshots, videos, and
  proprietary payloads are not committed or runtime dependencies. The local
  asset pack and decomp root used below are ignored/private verification inputs.

## Function and frame convention

The parser recognizes only the exact `intro-production-clean-frame` prefix.
The shared decimal-suffix parser requires at least one decimal digit, base-10
digits only, no sign, no trailing characters, and no conversion overflow. This
mode adds the safe inclusive bound `N <= 4096`; malformed or over-bound names
return an unsupported-mode error and do not render.

For a fresh process, the convention is:

1. Runtime initialization completes.
2. `TECMO_MODE_FIRST_SPRITE` is entered normally, yielding production title
   step 6, local mode frame 0, and global frame 0.
3. Exactly `N` calls to `tecmo_runtime_update` run with neutral production
   input.
4. The resulting state is rendered once by the generic
   `tecmo_runtime_render` caller.

Therefore `N=0` is the state before any update. Handoff updates are included in
the count: the update that reaches a production boundary resets the local mode
frame to 0 for the new step, and that post-update state is what is rendered.
The mode forces the debug overlay off and prints one sanitized state line:

```text
intro-production-state global=<global> step=<intro-step> local=<mode-frame> mode=<enum> attract=<0|1> title_armed=<0|1> title_confirming=<0|1> title_frame=<frame>
```

## Production boundary map

The first complete neutral-input cycle has these inclusive rendered ranges:

| Global `N` range | Intro step | Local frame | Boundary reached on next update |
| --- | ---: | ---: | --- |
| 0..132 | title | `N` | 133 -> license |
| 133..409 | license | `N-133` | 410 -> arena |
| 410..949 | arena | `N-410` | 950 -> READY |
| 950..1007 | READY | `N-950` | 1008 -> WARRIORS |
| 1008..1221 | WARRIORS | `N-1008` | 1222 -> CLIPPERS |
| 1222..1372 | CLIPPERS | `N-1222` | 1373 -> BUCKS |
| 1373..1455 | BUCKS | `N-1373` | 1456 -> PASS |
| 1456..1507 | PASS | `N-1456` | 1508 -> finale |
| 1508..2416 | finale | `N-1508` | 2417 -> attract |
| 2417..3058 | attract | `N-2417` | 3059 -> title reset |
| 3059.. | title (new cycle) | `N-3059` until the next handoff | normal cycle continues |

The boundary render run included all handoffs plus `N=4096`, which is inside a
second production cycle and demonstrates that the mode remains a normal
state-machine replay after the attract reset.

## Automated verification

All generated evidence below is ignored under `build/proof/`.

- Existing build: `.\build.ps1` passed and produced both console and Win32
  executables.
- Boundary renders: `N=0,1,132,133,409,410,949,950,1007,1008,1221,1222,
  1372,1373,1455,1456,1507,1508,2416,2417,3058,3059,4096` all rendered
  successfully. State/output log:
  `build/proof/production-boundary-renders-1.txt`.
- Parser/bound failures: missing suffix, alphabetic suffix, trailing
  character, plus sign, negative sign, `4097`, and
  `4294967296` all exited 1, printed `Unsupported render-test mode`, and
  created no PNG. Log: `build/proof/production-malformed-renders-1.txt`.
- Existing native flow: `--flow-test` passed with a valid local decomp root and
  local asset pack. No flow output is committed.
- Fresh-process determinism: two independent process launches matched SHA-256
  for every representative pair at `N=0,410,950,1508,2417,3059,4096`.

Representative deterministic PNG hashes from
`build/proof/production-deterministic-hashes-1.txt`:

| `N` | SHA-256 |
| ---: | --- |
| 0 | `2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A` |
| 410 | `1A14A50B0A32F3A76C57273898632B555BC7AEC14F970A6F3D02E0AB7934925D` |
| 1508 | `A8E29464165DA3185054F0BA155484F204C187818BCF84BBF63E45A60446B101` |
| 3059 | `2377B0FF24274E21F5963CC35E43D0F666B7626E890A23C01A7621B842055F9A` |
| 4096 | `C4EEE972CD1F6EC34D7170387A9F92C637089745DAC5C0983E21E8C2B00B6533` |

Production frames matched the existing per-scene clean modes byte-for-byte for
13 overlap cases, including arena frames 0/539, READY frames 0/57, WARRIORS
frames 0/213, CLIPPERS frames 0/150, BUCKS frames 0/82, PASS frames 0/51,
and finale frame 0. Comparison log:
`build/proof/production-overlap-hashes-1.txt`.

## Local proof commands

Use placeholders for private local paths; do not commit those paths or their
payloads:

```powershell
$env:TECMO_ASSETPACK = '<LOCAL_ASSETPACK>'
.\build\tecmo_port.exe --root '<LOCAL_DECOMP_ROOT>' `
  --render-test-mode intro-production-clean-frame0 `
  build\proof\intro-production-clean-frame0.png

.\build\tecmo_port.exe --root '<LOCAL_DECOMP_ROOT>' `
  --render-test-mode intro-production-clean-frame3059 `
  build\proof\intro-production-clean-frame3059.png

.\build\tecmo_port.exe --root '<LOCAL_DECOMP_ROOT>' --flow-test
```

The inspected full-resolution frames showed the expected black fade endpoints,
the intact arena bands/scoreboard, the CLIPPERS wordmark and player/ball art,
the finale shooter frame, the late attract logo, the black title-reset frame,
and a post-reset WARRIORS frame. No debug overlay was visible in the reviewed
frames.

## Limitations

The proof uses the existing native asset-pack runtime and validates the
production C state-machine path; it does not claim ROM-frame parity beyond the
documented native boundaries. The CLI mode's maximum is deliberately 4096
updates, not an unbounded replay facility. The generated PNGs and local
manifests are ignored evidence and must remain uncommitted.

## Lineage and merge handoff

- Required base/parent: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`.
- Worker branch: `codex/r4-frontend-intro-title-proof-luna`.
- Implementation commit SHA: `853e46ac3151cc80fdc432b9784e40e80f0edf1c`.
- Only the two paths listed in Scope may be included in the commit.

The orchestrator should review the source diff and proof logs, then cherry-pick
the reported implementation commit onto its branch. Do not merge, force-reset,
or push from this worker worktree.
