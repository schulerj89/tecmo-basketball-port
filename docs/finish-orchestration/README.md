# Finish Orchestration Control Plane

Authoritative state is under `state/`; schemas are under `schemas/`; repository
tools are under `tools/finish-orchestration/`.

`state/schedule.json` is the explicit capacity/readiness view for parallel Sol
domain lanes. It preserves one master authority while showing active, reserved,
dependency-blocked, and completed lanes before thread creation.

From the repository root:

```powershell
.\tools\finish-orchestration\Test-ControlPlane.ps1
.\tools\finish-orchestration\Show-ControlPlane.ps1
.\tools\finish-orchestration\Show-ControlPlane.ps1 -WriteDashboard
.\tools\finish-orchestration\Verify-Lineage.ps1
```

These commands validate coordination data only. They are not product tests and
must not be represented as gameplay, render, audio, rebuild, or release QA.

Use `Invoke-TaskTransition.ps1` for an audited state change. Review and commit
the resulting JSON and regenerated dashboard together.
