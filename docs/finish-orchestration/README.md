# Finish Orchestration Control Plane

Authoritative state is under `state/`; schemas are under `schemas/`; repository
tools are under `tools/finish-orchestration/`.

`state/schedule.json` is the explicit capacity/readiness view for parallel Sol
domain lanes. It preserves one master authority while showing active, reserved,
dependency-blocked, and completed lanes before thread creation.

`state/sessions.json` also supports a top-level `release_orchestrator` using
`gpt-5.6-terra` with `thinking=xhigh`. That role reports to the master, owns
guarded cross-round delivery and preview coordination, and may register one
bounded independent Luna/max integration-QA worker against an assigned task.
It does not replace the task's domain or integration Sol lineage.

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
