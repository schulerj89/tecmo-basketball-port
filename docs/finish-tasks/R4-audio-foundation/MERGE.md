# R4 Audio Foundation merge contract

Status: **Sol acceptance pending**. Do not merge until the authoritative Sol
task `019fc822-bdfa-7ab1-8b35-e7d9aa58969d` accepts this isolated foundation
and confirms the ignored terminal proof manifest.

## Branches and ancestry

- Writer worktree: `C:\Users\joshs\Projects\tecmo-basketball-port-r4-audio-foundation-luna`
- Writer branch: `codex/r4-audio-foundation-luna`
- Sol domain branch: `codex/r4-audio-foundation-sol`
- Exact base: `6d8f9c7a99a7ce188f1a523247d3a9b9093860fb`
- Required merge mode: fast-forward only, into the Sol domain worktree.
- No main-branch operation, push, rebase, or merge commit is part of this
  handoff.

The ordered pre-Revision-C commits are:

```text
29611607babe31415ab063520d832631ab3c2e4c
51790b832eb4bb23db07ac7965d6c2b1da877a1e
c8eb88a8155b1d00471c964646a7f56f89cc6540
4ddb1bf3f5fda1e207e14ed443367afb5796a644
f9499e43503e69cbbe2f16774d73a4964a34adfc
ad82eb9ba34316b568b3ca33c80949657a973893
```

Revision C adds script commit
`96471e42f79669379df0a573e8e82be037e559fb`, followed by the separate terminal
documentation record. The documentation record is described as the branch
HEAD after its commit; its exact SHA is intentionally not embedded in these
docs and is supplied by the post-commit ignored proof manifest and Sol handoff.

## Sol-side procedure

After acceptance, in the Sol domain worktree:

```powershell
git status --short --branch
git rev-parse HEAD
git merge-base 6d8f9c7a99a7ce188f1a523247d3a9b9093860fb HEAD
git merge --ff-only codex/r4-audio-foundation-luna
```

The expected pre-merge Sol worktree is clean, and the fast-forward base check
must identify the exact base above. If `git merge --ff-only` cannot advance the
Sol branch without a merge commit, stop and return the handoff for explicit
Sol resolution. Do not force the branch or alter either worktree destructively.

After the fast-forward, inspect the changed-path allowlist and rerun the build
and three owned suites with `TECMO_SKIP_SHORTCUT=1` and the local-private ROM
supplied through `TECMO_ROM_PATH`. Confirm that the ignored proof manifest’s
`proof_generation_head` equals the resulting Sol branch HEAD, and retain the
known artifact hashes from [PROOF.md](PROOF.md).

## Acceptance boundary

This merge carries the isolated R4 audio foundation only. It does not close
cross-domain cue call sites, full ACC-AUDIO integration, nonlinear/cycle-exact
NES APU differences, DMC reader phase, unresolved DMC IDs 0/1/2, neutral effect
5, or bounded-correlation-only effect 6. Sol acceptance must preserve those
explicit limitations.
