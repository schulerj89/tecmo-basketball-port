# Finish-Orchestration Tools

These scripts operate only on coordination metadata in
`docs/finish-orchestration`. They do not run product tests.

- `Validate-ControlPlane.ps1` validates every JSON file against its schema,
  replays task transitions, detects duplicate IDs and registry collisions,
  checks dependencies/references/recovery policy, validates accepted task docs
  in each task's registered worktree (with a repository-root fallback), and
  detects ownership overlap.
- `Verify-Lineage.ps1` verifies expected bases/parents, branch ancestry, result
  commits, and live worktree/branch registration.
- `Show-ControlPlane.ps1` prints status or generates/checks the Markdown
  dashboard.
- `Invoke-TaskTransition.ps1` applies one permitted audited queue transition.
- `Test-ControlPlane.ps1` runs primitive failure-detection self-tests, all
  schema/semantic/Git checks, and the stale-dashboard gate.

Run the generator once after state changes, then commit the JSON and dashboard
together:

```powershell
.\tools\finish-orchestration\Show-ControlPlane.ps1 -WriteDashboard -IncludeGit
.\tools\finish-orchestration\Test-ControlPlane.ps1
```
