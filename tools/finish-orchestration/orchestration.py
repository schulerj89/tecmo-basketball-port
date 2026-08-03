#!/usr/bin/env python3
"""Durable coordination validator and dashboard for Tecmo finish work.

This tool validates orchestration state only. It does not build, run, render,
capture, or otherwise test the product.
"""

from __future__ import annotations

import argparse
import collections
import copy
import datetime as dt
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Iterable

try:
    from jsonschema import Draft202012Validator, FormatChecker
except ImportError as exc:  # pragma: no cover - explicit operator guidance
    raise SystemExit(
        "Python package 'jsonschema' is required. Install jsonschema>=4.18 "
        "or use the repository's configured Python environment."
    ) from exc


STATE_SCHEMA_MAP = {
    "acceptance.json": "acceptance.schema.json",
    "blockers.json": "blockers.schema.json",
    "evidence.json": "evidence.schema.json",
    "inventory.json": "inventory.schema.json",
    "ownership.json": "ownership.schema.json",
    "queue.json": "queue.schema.json",
    "rounds.json": "rounds.schema.json",
    "schedule.json": "schedule.schema.json",
    "sessions.json": "sessions.schema.json",
    "state-machine.json": "state-machine.schema.json",
}

FINAL_OR_LATE_STATES = {
    "sol_accepted",
    "ready_for_round_staging",
    "combined_qa",
    "ready_for_main",
    "merged",
    "pushed",
}
INTEGRATION_SIGNOFF_STATES = {"ready_for_main", "merged", "pushed"}
ACTIVE_TASK_STATES = {
    "assigned",
    "in_progress",
    "sol_review",
    "luna_revision",
    "sol_accepted",
    "ready_for_round_staging",
    "combined_qa",
    "ready_for_main",
    "merged",
    "blocked",
    "reopened",
}
WRITABLE_OR_REVIEW_TASK_STATES = {
    "assigned",
    "in_progress",
    "sol_review",
    "luna_revision",
    "blocked",
    "reopened",
}
ACTIVE_SESSION_STATES = {"active", "idle", "blocked"}


class CheckResult:
    def __init__(self) -> None:
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    def extend(self, other: "CheckResult") -> None:
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)


def utc_now() -> str:
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_time(value: str) -> dt.datetime:
    return dt.datetime.fromisoformat(value.replace("Z", "+00:00"))


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ValueError(f"missing JSON file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON in {path}: {exc}") from exc


def write_json_atomic(path: Path, value: Any) -> None:
    encoded = json.dumps(value, indent=2, ensure_ascii=False) + "\n"
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=str(path.parent))
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(encoded)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def orchestration_paths(repo_root: Path) -> tuple[Path, Path, Path]:
    docs = repo_root / "docs" / "finish-orchestration"
    return docs / "state", docs / "schemas", docs / "dashboard.md"


def json_path(parts: Iterable[Any]) -> str:
    result = "$"
    for part in parts:
        if isinstance(part, int):
            result += f"[{part}]"
        else:
            result += f".{part}"
    return result


def validate_schemas(state_dir: Path, schema_dir: Path) -> CheckResult:
    result = CheckResult()
    actual_states = {path.name for path in state_dir.glob("*.json")}
    expected_states = set(STATE_SCHEMA_MAP)
    for missing in sorted(expected_states - actual_states):
        result.error(f"schema: missing state file {missing}")
    for extra in sorted(actual_states - expected_states):
        result.error(f"schema: state file {extra} has no registered schema")

    actual_schemas = {path.name for path in schema_dir.glob("*.schema.json")}
    expected_schemas = set(STATE_SCHEMA_MAP.values())
    for missing in sorted(expected_schemas - actual_schemas):
        result.error(f"schema: missing schema file {missing}")
    for extra in sorted(actual_schemas - expected_schemas):
        result.warn(f"schema: unregistered schema file {extra}")

    for state_name, schema_name in sorted(STATE_SCHEMA_MAP.items()):
        state_path = state_dir / state_name
        schema_path = schema_dir / schema_name
        if not state_path.is_file() or not schema_path.is_file():
            continue
        try:
            instance = load_json(state_path)
            schema = load_json(schema_path)
            Draft202012Validator.check_schema(schema)
            validator = Draft202012Validator(schema, format_checker=FormatChecker())
            errors = sorted(validator.iter_errors(instance), key=lambda item: list(item.absolute_path))
            for error in errors:
                result.error(f"schema: {state_name} {json_path(error.absolute_path)}: {error.message}")
        except (ValueError, Exception) as exc:
            # jsonschema raises several schema-specific subclasses. Preserve a
            # stable operator-facing message while allowing validation to continue.
            result.error(f"schema: {state_name}: {exc}")
    return result


def find_duplicates(values: Iterable[str]) -> list[str]:
    counts = collections.Counter(values)
    return sorted(value for value, count in counts.items() if count > 1)


def check_unique(records: list[dict[str, Any]], key: str, label: str, result: CheckResult) -> None:
    duplicates = find_duplicates(str(record[key]).casefold() for record in records)
    for duplicate in duplicates:
        result.error(f"duplicate {label}: {duplicate}")


def detect_dependency_cycles(task_by_id: dict[str, dict[str, Any]]) -> list[list[str]]:
    cycles: list[list[str]] = []
    visiting: list[str] = []
    state: dict[str, int] = {}

    def visit(task_id: str) -> None:
        marker = state.get(task_id, 0)
        if marker == 2:
            return
        if marker == 1:
            start = visiting.index(task_id)
            cycles.append(visiting[start:] + [task_id])
            return
        state[task_id] = 1
        visiting.append(task_id)
        for dependency in task_by_id[task_id]["dependencies"]:
            if dependency in task_by_id:
                visit(dependency)
        visiting.pop()
        state[task_id] = 2

    for identifier in task_by_id:
        visit(identifier)
    return cycles


def normalize_relative_path(value: str) -> str:
    normalized = value.replace("\\", "/").strip()
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def canonical_worktree(value: str) -> str:
    return os.path.normcase(os.path.abspath(value.replace("/", os.sep)))


def task_docs_candidates(repo_root: Path, task: dict[str, Any]) -> list[Path]:
    """Return accepted-doc locations in registered-context-first order."""
    relative = Path(normalize_relative_path(task["task_docs_path"]))
    roots: list[Path] = []
    if task.get("worktree"):
        roots.append(Path(canonical_worktree(task["worktree"])))
    resolved_repo_root = repo_root.resolve()
    if not roots or canonical_worktree(str(resolved_repo_root)) != canonical_worktree(str(roots[0])):
        roots.append(resolved_repo_root)
    return [root / relative for root in roots]


def tasks_may_share_sequential_context(first: dict[str, Any], second: dict[str, Any]) -> bool:
    """Allow exact branch/worktree reuse only for an explicit frozen same-Sol sequence."""
    same_sol = (
        first["sol_orchestrator_session_id"] is not None
        and first["sol_orchestrator_session_id"] == second["sol_orchestrator_session_id"]
    )
    dependency_ordered = (
        first["task_id"] in second["dependencies"]
        or second["task_id"] in first["dependencies"]
    )
    concurrently_writable = (
        first["state"] in WRITABLE_OR_REVIEW_TASK_STATES
        and second["state"] in WRITABLE_OR_REVIEW_TASK_STATES
    )
    frozen_delivery_handoff = (
        first["state"] == "ready_for_round_staging"
        and first["task_id"] in second["dependencies"]
    ) or (
        second["state"] == "ready_for_round_staging"
        and second["task_id"] in first["dependencies"]
    )
    return (
        same_sol
        and (first["round_id"] == second["round_id"] or frozen_delivery_handoff)
        and dependency_ordered
        and not concurrently_writable
    )


def glob_regex(pattern: str) -> re.Pattern[str]:
    pattern = normalize_relative_path(pattern)
    index = 0
    pieces = ["^"]
    while index < len(pattern):
        char = pattern[index]
        if char == "*":
            if index + 1 < len(pattern) and pattern[index + 1] == "*":
                index += 2
                if index < len(pattern) and pattern[index] == "/":
                    pieces.append("(?:.*/)?")
                    index += 1
                else:
                    pieces.append(".*")
                continue
            pieces.append("[^/]*")
        elif char == "?":
            pieces.append("[^/]")
        elif char == "[":
            close = pattern.find("]", index + 1)
            if close == -1:
                pieces.append("\\[")
            else:
                content = pattern[index + 1 : close]
                if content.startswith("!"):
                    content = "^" + content[1:]
                pieces.append("[" + content + "]")
                index = close
        else:
            pieces.append(re.escape(char))
        index += 1
    pieces.append("$")
    return re.compile("".join(pieces), re.IGNORECASE if os.name == "nt" else 0)


def matches_glob(path: str, pattern: str) -> bool:
    return bool(glob_regex(pattern).match(normalize_relative_path(path)))


def static_glob_prefix(pattern: str) -> str:
    pattern = normalize_relative_path(pattern)
    positions = [position for position in (pattern.find("*"), pattern.find("?"), pattern.find("[")) if position >= 0]
    if not positions:
        return pattern
    return pattern[: min(positions)]


def possible_pattern_overlap(left: str, right: str) -> bool:
    left = normalize_relative_path(left)
    right = normalize_relative_path(right)
    left_magic = any(token in left for token in "*?[")
    right_magic = any(token in right for token in "*?[")
    if not left_magic and not right_magic:
        return left.casefold() == right.casefold()
    if not left_magic:
        return matches_glob(left, right)
    if not right_magic:
        return matches_glob(right, left)

    lp = static_glob_prefix(left).casefold()
    rp = static_glob_prefix(right).casefold()
    if not (lp.startswith(rp) or rp.startswith(lp)):
        return False

    longer = static_glob_prefix(left) if len(lp) >= len(rp) else static_glob_prefix(right)
    probes = [
        longer.rstrip("/") + "/__ownership_probe__.c",
        longer.rstrip("/") + "/__ownership_probe__/file.c",
        longer.rstrip("/") + ".c",
        longer.rstrip("/") + "/README.md",
    ]
    if any(matches_glob(probe, left) and matches_glob(probe, right) for probe in probes):
        return True

    # Conservatively reject common/ancestor wildcard namespaces even if a
    # synthetic probe could not witness a suffix constraint. Tasks can rescope
    # to disjoint prefixes or declare a controlled shared boundary.
    return lp.endswith("/") or rp.endswith("/") or "**" in left or "**" in right


def git_output(repo_root: Path, *arguments: str, check: bool = True) -> str:
    process = subprocess.run(
        ["git", "-C", str(repo_root), *arguments],
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and process.returncode != 0:
        detail = process.stderr.strip() or process.stdout.strip()
        raise RuntimeError(f"git {' '.join(arguments)} failed: {detail}")
    return process.stdout.strip()


def tracked_paths(repo_root: Path) -> set[str]:
    try:
        output = git_output(repo_root, "ls-files", "-z")
    except RuntimeError:
        return set()
    return {normalize_relative_path(item) for item in output.split("\0") if item}


def boundary_allows(
    boundaries: list[dict[str, Any]],
    left: dict[str, Any],
    right: dict[str, Any],
    witness: str | None,
) -> bool:
    task_pair = {left["task_id"], right["task_id"]}
    for boundary in boundaries:
        if boundary["round_id"] != left["round_id"]:
            continue
        if witness and not matches_glob(witness, boundary["writable_glob"]):
            continue
        if not witness and not any(
            possible_pattern_overlap(boundary["writable_glob"], pattern)
            for pattern in left["writable_globs"] + right["writable_globs"]
        ):
            continue
        if boundary["policy"] == "sequential" and task_pair.issubset(set(boundary["ordered_task_ids"])):
            return True
        if boundary["policy"] == "designated_task" and boundary["designated_task_id"] in task_pair:
            return True
    return False


def ownership_overlaps(repo_root: Path, ownership: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    claims = [claim for claim in ownership["claims"] if claim["active"]]
    candidates = tracked_paths(repo_root)
    for claim in claims:
        candidates.update(normalize_relative_path(path) for path in claim["planned_paths"])

    for index, left in enumerate(claims):
        for right in claims[index + 1 :]:
            if left["task_id"] == right["task_id"] or left["round_id"] != right["round_id"]:
                continue
            witness = next(
                (
                    path
                    for path in sorted(candidates)
                    if any(matches_glob(path, pattern) for pattern in left["writable_globs"])
                    and any(matches_glob(path, pattern) for pattern in right["writable_globs"])
                ),
                None,
            )
            possible = witness is not None or any(
                possible_pattern_overlap(a, b)
                for a in left["writable_globs"]
                for b in right["writable_globs"]
            )
            if not possible or boundary_allows(ownership["shared_boundaries"], left, right, witness):
                continue
            detail = f" at {witness}" if witness else " (possible glob intersection)"
            errors.append(
                f"ownership overlap in {left['round_id']}: {left['claim_id']} ({left['task_id']}) "
                f"and {right['claim_id']} ({right['task_id']}){detail}"
            )
    return errors


def load_all_state(state_dir: Path) -> dict[str, Any]:
    return {name.removesuffix(".json").replace("-", "_"): load_json(state_dir / name) for name in STATE_SCHEMA_MAP}


def validate_semantics(repo_root: Path, state: dict[str, Any]) -> CheckResult:
    result = CheckResult()
    machine = state["state_machine"]
    queue = state["queue"]
    rounds = state["rounds"]
    sessions = state["sessions"]
    ownership = state["ownership"]
    evidence = state["evidence"]
    blockers = state["blockers"]
    acceptance = state["acceptance"]
    inventory = state["inventory"]
    schedule = state["schedule"]

    states = set(machine["states"])
    if machine["initial_state"] not in states:
        result.error("state machine initial_state is not declared")
    transition_rows = machine["transitions"]
    check_unique(transition_rows, "from", "transition source", result)
    transitions = {row["from"]: set(row["to"]) for row in transition_rows}
    if set(transitions) != states:
        result.error("state machine must provide exactly one transition row for every declared state")
    for source, targets in transitions.items():
        for target in targets:
            if target not in states:
                result.error(f"state machine transition {source} -> {target} targets an undeclared state")
    for terminal in machine["terminal_states"]:
        if terminal not in states:
            result.error(f"terminal state {terminal} is undeclared")

    tasks = queue["tasks"]
    round_rows = rounds["rounds"]
    session_rows = sessions["sessions"]
    worker_rows = sessions["reported_luna_workers"]
    claims = ownership["claims"]
    evidence_rows = evidence["records"]
    blocker_rows = blockers["blockers"]
    criteria = acceptance["criteria"]
    lane_rows = schedule["lanes"]

    for records, key, label in (
        (tasks, "task_id", "task id"),
        (round_rows, "round_id", "round id"),
        (session_rows, "session_id", "session id"),
        (session_rows, "thread_id", "thread id"),
        (worker_rows, "worker_id", "reported Luna worker id"),
        (worker_rows, "thread_id", "reported Luna thread id"),
        (claims, "claim_id", "ownership claim id"),
        (evidence_rows, "evidence_id", "evidence id"),
        (blocker_rows, "blocker_id", "blocker id"),
        (criteria, "criterion_id", "acceptance criterion id"),
        (inventory["threads"], "thread_id", "inventory thread id"),
        (inventory["worktrees"], "path", "inventory worktree path"),
        (inventory["branches"], "name", "inventory branch"),
        (inventory["commits"], "sha", "inventory commit"),
        (inventory["proof_artifacts"], "proof_id", "inventory proof id"),
        (lane_rows, "lane_id", "schedule lane id"),
    ):
        check_unique(records, key, label, result)

    task_by_id = {row["task_id"]: row for row in tasks}
    round_by_id = {row["round_id"]: row for row in round_rows}
    session_by_id = {row["session_id"]: row for row in session_rows}
    worker_by_id = {row["worker_id"]: row for row in worker_rows}
    claim_by_id = {row["claim_id"]: row for row in claims}
    evidence_by_id = {row["evidence_id"]: row for row in evidence_rows}
    blocker_by_id = {row["blocker_id"]: row for row in blocker_rows}

    if schedule["master_session_id"] != sessions["master_session_id"]:
        result.error("schedule master_session_id differs from sessions master_session_id")
    active_lane_count = sum(row["readiness"] == "active" for row in lane_rows)
    cleared_lane_count = sum(row["readiness"] == "cleared_for_creation" for row in lane_rows)
    policy_target = schedule["policy"]["target_active_domain_orchestrators"]
    monitoring_limit = schedule["policy"]["max_monitored_sol_orchestrators"]
    capacity = schedule["capacity"]
    expected_target_slots = max(0, policy_target - active_lane_count - cleared_lane_count)
    expected_monitoring_slots = max(0, monitoring_limit - active_lane_count - cleared_lane_count)
    if capacity["active_domain_orchestrators"] != active_lane_count:
        result.error("schedule capacity active_domain_orchestrators does not match active lanes")
    if capacity["cleared_for_creation"] != cleared_lane_count:
        result.error("schedule capacity cleared_for_creation does not match cleared lanes")
    if capacity["target_active_domain_orchestrators"] != policy_target:
        result.error("schedule capacity target differs from policy target")
    if capacity["monitoring_limit"] != monitoring_limit:
        result.error("schedule capacity monitoring limit differs from policy")
    if capacity["unallocated_target_slots"] != expected_target_slots:
        result.error("schedule capacity unallocated_target_slots is stale")
    if capacity["unallocated_monitoring_slots"] != expected_monitoring_slots:
        result.error("schedule capacity unallocated_monitoring_slots is stale")
    if active_lane_count + cleared_lane_count > monitoring_limit:
        result.error("schedule active and cleared lanes exceed monitoring capacity")

    lane_branches: dict[str, str] = {}
    lane_worktrees: dict[str, str] = {}
    for lane in lane_rows:
        lane_id = lane["lane_id"]
        for round_id in lane["round_ids"]:
            if round_id not in round_by_id:
                result.error(f"schedule lane {lane_id}: unknown round {round_id}")
        for task_id in lane["task_ids"]:
            if task_id not in task_by_id:
                result.error(f"schedule lane {lane_id}: unknown task {task_id}")
            elif task_by_id[task_id]["round_id"] not in lane["round_ids"]:
                result.error(f"schedule lane {lane_id}: task {task_id} is outside the lane rounds")
        reservation_owner = lane["reservation_owner_session_id"]
        if reservation_owner not in session_by_id:
            result.error(f"schedule lane {lane_id}: unknown reservation owner {reservation_owner}")
        orchestrator_id = lane["orchestrator_session_id"]
        orchestrator = session_by_id.get(orchestrator_id) if orchestrator_id else None
        if orchestrator_id and not orchestrator:
            result.error(f"schedule lane {lane_id}: unknown orchestrator {orchestrator_id}")
        elif orchestrator and orchestrator["role"] not in {"domain_orchestrator", "integration_orchestrator"}:
            result.error(f"schedule lane {lane_id}: assigned orchestrator is not a Sol orchestrator role")
        if lane["readiness"] == "active":
            if not orchestrator:
                result.error(f"schedule lane {lane_id}: active lane has no orchestrator")
            elif orchestrator["status"] not in ACTIVE_SESSION_STATES:
                result.error(f"schedule lane {lane_id}: active lane orchestrator is not active/idle/blocked")
        if lane["readiness"] == "cleared_for_creation" and orchestrator_id is not None:
            result.error(f"schedule lane {lane_id}: cleared-for-creation lane already has an orchestrator")
        if (lane["branch"] is None) != (lane["worktree"] is None):
            result.error(f"schedule lane {lane_id}: branch and worktree must both be set or both be null")
        if lane["branch"]:
            branch_key = lane["branch"].casefold()
            if branch_key in lane_branches:
                result.error(f"schedule lanes {lane_branches[branch_key]} and {lane_id} share branch {lane['branch']}")
            lane_branches[branch_key] = lane_id
        if lane["worktree"]:
            worktree_key = canonical_worktree(lane["worktree"])
            if worktree_key in lane_worktrees:
                result.error(f"schedule lanes {lane_worktrees[worktree_key]} and {lane_id} share worktree {lane['worktree']}")
            lane_worktrees[worktree_key] = lane_id
        expected_claim_session = orchestrator_id if lane["readiness"] == "active" else reservation_owner
        for claim_id in lane["ownership_claim_ids"]:
            claim = claim_by_id.get(claim_id)
            if not claim:
                result.error(f"schedule lane {lane_id}: unknown ownership claim {claim_id}")
                continue
            if claim["task_id"] not in lane["task_ids"]:
                result.error(f"schedule lane {lane_id}: claim {claim_id} belongs to a task outside the lane")
            if lane["readiness"] in {"active", "cleared_for_creation"} and not claim["active"]:
                result.error(f"schedule lane {lane_id}: active/cleared claim {claim_id} is not active")
            if lane["readiness"] in {"active", "cleared_for_creation"} and claim["session_id"] != expected_claim_session:
                result.error(f"schedule lane {lane_id}: claim {claim_id} is not held by the expected session")

    if sessions["master_session_id"] not in session_by_id:
        result.error("master_session_id does not reference a registered session")
    elif session_by_id[sessions["master_session_id"]]["role"] != "master":
        result.error("master_session_id does not reference a master role")
    session_thread_ids = {row["thread_id"] for row in session_rows}
    for worker in worker_rows:
        if worker["thread_id"] in session_thread_ids:
            result.error(f"reported Luna thread {worker['thread_id']} duplicates a registered Sol/master thread")

    active_task_branches: dict[str, dict[str, Any]] = {}
    active_task_worktrees: dict[str, dict[str, Any]] = {}
    for task in tasks:
        task_id = task["task_id"]
        if task["state"] not in states:
            result.error(f"task {task_id}: undeclared state {task['state']}")
        if task["round_id"] not in round_by_id:
            result.error(f"task {task_id}: unknown round {task['round_id']}")
        sol_session_id = task["sol_orchestrator_session_id"]
        if sol_session_id is not None and sol_session_id not in session_by_id:
            result.error(f"task {task_id}: unknown Sol session {sol_session_id}")
        if task["state"] in ACTIVE_TASK_STATES and sol_session_id is None:
            result.error(f"task {task_id}: active task has no assigned Sol orchestrator")
        for dependency in task["dependencies"]:
            if dependency not in task_by_id:
                result.error(f"task {task_id}: unknown dependency {dependency}")
            if dependency == task_id:
                result.error(f"task {task_id}: self dependency")
            if (
                task["state"] in ACTIVE_TASK_STATES
                and dependency in task_by_id
                and task_by_id[dependency]["state"] not in FINAL_OR_LATE_STATES
            ):
                result.error(
                    f"task {task_id}: active state {task['state']} has unsatisfied dependency "
                    f"{dependency} in state {task_by_id[dependency]['state']}"
                )
        for claim_id in task["ownership_claim_ids"]:
            claim = claim_by_id.get(claim_id)
            if not claim:
                result.error(f"task {task_id}: unknown ownership claim {claim_id}")
            elif claim["task_id"] != task_id:
                result.error(f"task {task_id}: ownership claim {claim_id} belongs to {claim['task_id']}")
        for evidence_id in task["evidence_ids"]:
            if evidence_id not in evidence_by_id:
                result.error(f"task {task_id}: unknown evidence {evidence_id}")
        for blocker_id in task["blocker_ids"]:
            blocker = blocker_by_id.get(blocker_id)
            if not blocker:
                result.error(f"task {task_id}: unknown blocker {blocker_id}")
            elif blocker["task_id"] != task_id:
                result.error(f"task {task_id}: blocker {blocker_id} belongs to {blocker['task_id']}")
        if task["state"] == "replaced" and not task["replacement_task_id"]:
            result.error(f"task {task_id}: replaced state requires replacement_task_id")
        if task["replacement_task_id"] and task["replacement_task_id"] not in task_by_id:
            result.error(f"task {task_id}: unknown replacement {task['replacement_task_id']}")
        if task["state"] in ACTIVE_TASK_STATES and not task["ownership_claim_ids"]:
            result.error(f"task {task_id}: active task has no ownership claim")

        audit = task["audit"]
        previous: str | None = None
        previous_time: dt.datetime | None = None
        for index, event in enumerate(audit):
            try:
                event_time = parse_time(event["timestamp"])
                if previous_time and event_time < previous_time:
                    result.error(f"task {task_id}: audit timestamp decreases at event {index}")
                previous_time = event_time
            except ValueError:
                pass  # schema validator reports invalid date-time syntax
            if event["actor_session_id"] not in session_by_id:
                result.error(f"task {task_id}: audit event {index} references unknown actor {event['actor_session_id']}")
            if index == 0:
                if event["from_state"] is not None or event["to_state"] != machine["initial_state"]:
                    result.error(f"task {task_id}: first audit event must initialize {machine['initial_state']} from null")
            else:
                if event["from_state"] != previous:
                    result.error(f"task {task_id}: audit event {index} from_state does not continue prior state")
                if event["to_state"] not in transitions.get(str(previous), set()):
                    result.error(f"task {task_id}: forbidden audit transition {previous} -> {event['to_state']}")
            previous = event["to_state"]
        if previous != task["state"]:
            result.error(f"task {task_id}: current state {task['state']} does not match audit tail {previous}")

        for worker_id in task["reported_luna_worker_session_ids"]:
            worker = worker_by_id.get(worker_id)
            if not worker:
                result.error(f"task {task_id}: reported Luna session {worker_id} is not registered")
            elif worker["reported_by_session_id"] != sol_session_id:
                result.error(f"task {task_id}: reported Luna session {worker_id} belongs to another Sol orchestrator")

        if task["state"] in FINAL_OR_LATE_STATES and not task["coordination_only"]:
            if not task["qa"]["sol_signoff"]:
                result.error(f"task {task_id}: {task['state']} requires Sol QA sign-off")
            docs_path = normalize_relative_path(task["task_docs_path"])
            if (
                not docs_path
                or docs_path.startswith("/")
                or re.match(r"^[A-Za-z]:", docs_path)
                or ".." in docs_path.split("/")
            ):
                result.error(f"task {task_id}: accepted task documentation path is unsafe/non-relative")
            elif not any(candidate.is_dir() for candidate in task_docs_candidates(repo_root, task)):
                result.error(
                    f"task {task_id}: accepted task documentation folder is missing from its registered "
                    "worktree and the validator repository root"
                )
        if task["state"] in INTEGRATION_SIGNOFF_STATES and not task["qa"]["integration_signoff"]:
            result.error(f"task {task_id}: {task['state']} requires integration sign-off")
        if task["state"] == "pushed":
            if task["merge"]["status"] != "pushed" or not task["merge"]["push_sha"] or not task["merge"]["pushed_at"]:
                result.error(f"task {task_id}: pushed state lacks complete merge/push record")

        if task["state"] in ACTIVE_TASK_STATES:
            if not task["branch"] or not task["worktree"]:
                result.error(f"task {task_id}: active task requires a branch and worktree")
                continue
            branch_key = task["branch"].casefold()
            worktree_key = canonical_worktree(task["worktree"])
            branch_peer = active_task_branches.get(branch_key)
            if branch_peer and not (
                tasks_may_share_sequential_context(branch_peer, task)
                and canonical_worktree(branch_peer["worktree"]) == worktree_key
            ):
                result.error(
                    f"active tasks {branch_peer['task_id']} and {task_id} share branch {task['branch']} "
                    "without an allowed exact sequential context"
                )
            worktree_peer = active_task_worktrees.get(worktree_key)
            if worktree_peer and not (
                tasks_may_share_sequential_context(worktree_peer, task)
                and worktree_peer["branch"].casefold() == branch_key
            ):
                result.error(
                    f"active tasks {worktree_peer['task_id']} and {task_id} share worktree {task['worktree']} "
                    "without an allowed exact sequential context"
                )
            active_task_branches[branch_key] = task
            active_task_worktrees[worktree_key] = task

    for cycle in detect_dependency_cycles(task_by_id):
        result.error("task dependency cycle: " + " -> ".join(cycle))

    active_session_branches: dict[str, str] = {}
    active_session_worktrees: dict[str, str] = {}
    for session in session_rows:
        session_id = session["session_id"]
        role = session["role"]
        if role in {"master", "domain_orchestrator", "integration_orchestrator"}:
            if session["model"] != "gpt-5.6-sol" or session["thinking"] != "max":
                result.error(f"session {session_id}: Sol role must use gpt-5.6-sol thinking=max")
        elif role == "security_operator":
            if session["model"] != "gpt-5.6-sol" or session["thinking"] != "high":
                result.error(f"session {session_id}: security operator must use gpt-5.6-sol thinking=high")
            if session["round_ids"] or session["task_ids"]:
                result.error(f"session {session_id}: security operator must remain outside product rounds/tasks")
        elif role == "luna_worker":
            if session["model"] != "gpt-5.6-luna" or session["thinking"] != "max":
                result.error(f"session {session_id}: worker must use gpt-5.6-luna thinking=max")
        elif role == "emergency_blocker":
            if session["model"] != "gpt-5.6-luna" or session["thinking"] != "high":
                result.error(f"session {session_id}: blocker role must use gpt-5.6-luna thinking=high")
            if session["title"] != "Blockers we need help with":
                result.error(f"session {session_id}: blocker role must use the exact emergency title")
        else:
            if "tecmo" not in session["title"].casefold():
                result.error(f"session {session_id}: title does not contain Tecmo")
            expected_tier = "sol" if session["model"] == "gpt-5.6-sol" else "luna"
            if expected_tier not in session["title"].casefold():
                result.error(f"session {session_id}: title does not contain model tier {expected_tier}")
        if session["status"] in ACTIVE_SESSION_STATES and session["pin_state"] != "pinned":
            result.error(f"session {session_id}: active/idle/blocked session must be pinned")
        for task_id in session["task_ids"]:
            if task_id not in task_by_id:
                result.error(f"session {session_id}: unknown task {task_id}")
        for round_id in session["round_ids"]:
            if round_id not in round_by_id:
                result.error(f"session {session_id}: unknown round {round_id}")
        for related in session["reported_luna_lineage"]:
            worker = worker_by_id.get(related)
            if not worker:
                result.error(f"session {session_id}: unknown reported Luna lineage {related}")
            elif worker["reported_by_session_id"] != session_id:
                result.error(f"session {session_id}: Luna lineage {related} reports to another Sol")
        for related in session["recovery_lineage"]:
            if related not in session_by_id:
                result.error(f"session {session_id}: unknown recovery lineage session {related}")
        for field in ("reports_to_session_id", "parent_session_id"):
            related = session[field]
            if related and related not in session_by_id:
                result.error(f"session {session_id}: unknown {field} {related}")
        if session["failure"]["count"] != len(session["failure"]["raw_error_signatures"]):
            result.error(f"session {session_id}: failure count does not equal recorded raw signatures")
        if session["status"] in ACTIVE_SESSION_STATES:
            if role in {"emergency_blocker", "security_operator"}:
                if session["branch"] or session["worktree"]:
                    result.error(f"session {session_id}: projectless operator role must not own a branch/worktree")
                continue
            if not session["branch"] or not session["worktree"]:
                result.error(f"session {session_id}: active session requires a branch and worktree")
                continue
            branch_key = session["branch"].casefold()
            worktree_key = canonical_worktree(session["worktree"])
            if branch_key in active_session_branches:
                result.error(f"active sessions {active_session_branches[branch_key]} and {session_id} share branch {session['branch']}")
            if worktree_key in active_session_worktrees:
                result.error(f"active sessions {active_session_worktrees[worktree_key]} and {session_id} share worktree {session['worktree']}")
            active_session_branches[branch_key] = session_id
            active_session_worktrees[worktree_key] = session_id

    for worker in worker_rows:
        worker_id = worker["worker_id"]
        if worker["model"] != "gpt-5.6-luna" or worker["thinking"] != "max":
            result.error(f"reported worker {worker_id}: must use gpt-5.6-luna thinking=max")
        if "tecmo" not in worker["title"].casefold() or "luna" not in worker["title"].casefold():
            result.error(f"reported worker {worker_id}: title must contain Tecmo and Luna")
        reporter = session_by_id.get(worker["reported_by_session_id"])
        if not reporter or reporter["role"] not in {"domain_orchestrator", "integration_orchestrator"}:
            result.error(f"reported worker {worker_id}: reporter is not a registered Sol orchestrator")
        for task_id in worker["task_ids"]:
            if task_id not in task_by_id:
                result.error(f"reported worker {worker_id}: unknown task {task_id}")
        for round_id in worker["round_ids"]:
            if round_id not in round_by_id:
                result.error(f"reported worker {worker_id}: unknown round {round_id}")
        for related in worker["recovery_lineage"]:
            if related not in worker_by_id:
                result.error(f"reported worker {worker_id}: unknown worker recovery lineage {related}")
        if worker["status"] in ACTIVE_SESSION_STATES and worker["pin_state"] != "pinned":
            result.error(f"reported worker {worker_id}: active/idle/blocked worker must be pinned")
        if worker["failure"]["count"] != len(worker["failure"]["raw_error_signatures"]):
            result.error(f"reported worker {worker_id}: failure count does not equal recorded signatures")
        has_branch = worker["branch"] is not None
        has_worktree = worker["worktree"] is not None
        if worker["status"] in ACTIVE_SESSION_STATES and has_branch and not has_worktree:
            result.error(f"reported worker {worker_id}: a branch requires a registered worktree")
        if worker["status"] in ACTIVE_SESSION_STATES and has_worktree:
            if worker["base_sha"] is None or worker["last_good_sha"] is None:
                result.error(f"reported worker {worker_id}: active Git context requires base_sha and last_good_sha")
            worktree_key = canonical_worktree(worker["worktree"])
            if worktree_key in active_session_worktrees:
                result.error(
                    f"reported worker {worker_id} shares worktree {worker['worktree']} with active context "
                    f"{active_session_worktrees[worktree_key]}"
                )
            active_session_worktrees[worktree_key] = worker_id
            if has_branch:
                branch_key = worker["branch"].casefold()
                if branch_key in active_session_branches:
                    result.error(
                        f"reported worker {worker_id} shares branch {worker['branch']} with active context "
                        f"{active_session_branches[branch_key]}"
                    )
                active_session_branches[branch_key] = worker_id

    for claim in claims:
        claim_id = claim["claim_id"]
        task = task_by_id.get(claim["task_id"])
        if not task:
            result.error(f"claim {claim_id}: unknown task {claim['task_id']}")
        elif task["round_id"] != claim["round_id"]:
            result.error(f"claim {claim_id}: round differs from its task")
        if claim["round_id"] not in round_by_id:
            result.error(f"claim {claim_id}: unknown round {claim['round_id']}")
        if claim["session_id"] not in session_by_id:
            result.error(f"claim {claim_id}: unknown session {claim['session_id']}")
        for pattern in claim["writable_globs"]:
            normalized = normalize_relative_path(pattern)
            if not normalized or normalized.startswith("/") or re.match(r"^[A-Za-z]:", normalized) or ".." in normalized.split("/"):
                result.error(f"claim {claim_id}: unsafe/non-relative writable glob {pattern}")
        for path in claim["planned_paths"]:
            normalized = normalize_relative_path(path)
            if not normalized or normalized.startswith("/") or re.match(r"^[A-Za-z]:", normalized) or ".." in normalized.split("/"):
                result.error(f"claim {claim_id}: unsafe/non-relative planned path {path}")

    for overlap in ownership_overlaps(repo_root, ownership):
        result.error(overlap)

    merge_order_by_round = {row["round_id"]: row for row in ownership["merge_orders"]}
    check_unique(ownership["merge_orders"], "round_id", "ownership merge-order round", result)
    for round_row in round_rows:
        round_id = round_row["round_id"]
        included = set(round_row["included_task_ids"])
        for task_id in included:
            if task_id not in task_by_id:
                result.error(f"round {round_id}: unknown included task {task_id}")
            elif task_by_id[task_id]["round_id"] != round_id:
                result.error(f"round {round_id}: included task {task_id} belongs to another round")
        if set(round_row["merge_order"]) != included:
            result.error(f"round {round_id}: merge_order must contain each included task exactly once")
        declaration = merge_order_by_round.get(round_id)
        if not declaration:
            result.error(f"round {round_id}: no ownership merge-order declaration")
        elif declaration["ordered_task_ids"] != round_row["merge_order"]:
            result.error(f"round {round_id}: ownership and round merge orders differ")
        for session_id in round_row["sol_orchestrator_session_ids"]:
            if session_id not in session_by_id:
                result.error(f"round {round_id}: unknown Sol orchestrator {session_id}")
        integration_id = round_row["integration_qa_session_id"]
        if integration_id and integration_id not in session_by_id:
            result.error(f"round {round_id}: unknown integration QA session {integration_id}")
        if round_row["combined_qa"]["required"] and round_row["combined_qa"]["status"] == "accepted":
            if not round_row["combined_qa"]["accepted_sha"] or not round_row["combined_qa"]["report_path"]:
                result.error(f"round {round_id}: accepted combined QA lacks SHA/report")
        if round_row["status"] == "pushed":
            integration = round_row["main_integration"]
            if integration["push_result"] != "succeeded" or not integration["push_sha"] or not integration["pushed_at"]:
                result.error(f"round {round_id}: pushed status lacks successful push record")

    for record in evidence_rows:
        for task_id in record["task_ids"]:
            if task_id not in task_by_id:
                result.error(f"evidence {record['evidence_id']}: unknown task {task_id}")
        if record["kind"] in {"asm", "rom_table"} and not record["asm"]:
            result.error(f"evidence {record['evidence_id']}: ASM/ROM-table evidence requires bank/address/routine")
        if record["local_private"] and not record["limits"]:
            result.error(f"evidence {record['evidence_id']}: private evidence requires limits")

    for criterion in criteria:
        criterion_id = criterion["criterion_id"]
        for task_id in criterion["task_ids"]:
            if task_id not in task_by_id:
                result.error(f"criterion {criterion_id}: unknown task {task_id}")
        for evidence_id in criterion["evidence_ids"]:
            if evidence_id not in evidence_by_id:
                result.error(f"criterion {criterion_id}: unknown evidence {evidence_id}")
        if criterion["classification"] == "native_approximation_with_justification" and not criterion["justification"]:
            result.error(f"criterion {criterion_id}: approximation requires justification")
        if criterion["status"] == "accepted":
            if criterion["classification"] == "incomplete":
                result.error(f"criterion {criterion_id}: accepted criterion cannot remain incomplete")
            if not criterion["accepted_by_session_id"] or not criterion["accepted_at"]:
                result.error(f"criterion {criterion_id}: accepted criterion lacks signer/time")
    incomplete = [row for row in criteria if row["classification"] == "incomplete"]
    if acceptance["project_status"] == "complete" and incomplete:
        result.error("project_status is complete while acceptance criteria remain incomplete")

    open_blockers = [row for row in blocker_rows if row["status"] == "open"]
    if open_blockers and (not blockers["emergency_thread_id"] or not blockers["emergency_session_id"]):
        result.error("open external blockers require the emergency thread/session registry")
    if not open_blockers and (blockers["emergency_thread_id"] or blockers["emergency_session_id"]):
        result.error("emergency blocker thread/session must be cleared when no blocker is open")
    for blocker in blocker_rows:
        if blocker["task_id"] not in task_by_id:
            result.error(f"blocker {blocker['blocker_id']}: unknown task {blocker['task_id']}")
        if blocker["category"] not in blockers["policy"]["allowed_categories"]:
            result.error(f"blocker {blocker['blocker_id']}: category is not allowed by policy")

    if inventory["inventory_status"] == "complete":
        if inventory["snapshot_started_at"] is None or inventory["snapshot_completed_at"] is None:
            result.error("complete inventory requires start and completion timestamps")
        if inventory["unresolved_items"]:
            result.error("complete inventory cannot retain unresolved_items")

    return result


def git_commit_exists(repo_root: Path, sha: str) -> bool:
    process = subprocess.run(
        ["git", "-C", str(repo_root), "cat-file", "-e", f"{sha}^{{commit}}"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return process.returncode == 0


def git_is_ancestor(repo_root: Path, ancestor: str, descendant: str) -> bool:
    process = subprocess.run(
        ["git", "-C", str(repo_root), "merge-base", "--is-ancestor", ancestor, descendant],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return process.returncode == 0


def parse_worktrees(repo_root: Path) -> dict[str, dict[str, str]]:
    output = git_output(repo_root, "worktree", "list", "--porcelain")
    rows: dict[str, dict[str, str]] = {}
    current: dict[str, str] = {}
    for line in output.splitlines() + [""]:
        if not line:
            if current.get("worktree"):
                rows[canonical_worktree(current["worktree"])] = current
            current = {}
            continue
        key, _, value = line.partition(" ")
        current[key] = value
    return rows


def validate_git_lineage(repo_root: Path, state: dict[str, Any]) -> CheckResult:
    result = CheckResult()
    queue = state["queue"]
    sessions = state["sessions"]
    inventory = state["inventory"]
    try:
        git_output(repo_root, "rev-parse", "--git-dir")
        worktrees = parse_worktrees(repo_root)
    except RuntimeError as exc:
        result.error(f"lineage: {exc}")
        return result

    for sha_name, sha in (("program_base_sha", queue["program_base_sha"]), ("inventory snapshot base", inventory["snapshot_base_sha"])):
        if not git_commit_exists(repo_root, sha):
            result.error(f"lineage: {sha_name} {sha} does not exist")

    for task in queue["tasks"]:
        task_id = task["task_id"]
        for field in ("base_sha", "expected_parent_sha"):
            if not git_commit_exists(repo_root, task[field]):
                result.error(f"lineage: task {task_id} {field} {task[field]} does not exist")
        for commit in task["result_commits"]:
            if not git_commit_exists(repo_root, commit):
                result.error(f"lineage: task {task_id} result commit {commit} does not exist")
            elif not git_is_ancestor(repo_root, task["base_sha"], commit):
                result.error(f"lineage: task {task_id} result commit {commit} does not descend from base")

        if task["state"] == "pushed":
            main_tip = git_output(repo_root, "rev-parse", "main")
            for commit in task["result_commits"]:
                if git_commit_exists(repo_root, commit) and not git_is_ancestor(repo_root, commit, main_tip):
                    result.error(f"lineage: pushed task {task_id} result commit {commit} is not on main")
            continue
        if task["state"] in {"backlog", "scoped", "failed", "replaced"}:
            continue
        if not task["branch"] or not task["worktree"]:
            result.error(f"lineage: active task {task_id} has no branch/worktree")
            continue
        try:
            branch_tip = git_output(repo_root, "rev-parse", f"refs/heads/{task['branch']}")
        except RuntimeError as exc:
            result.error(f"lineage: task {task_id}: {exc}")
            continue
        if not git_is_ancestor(repo_root, task["base_sha"], branch_tip):
            result.error(f"lineage: task {task_id} branch tip does not descend from base")
        try:
            # Integration branches may merge a newly advanced main as a second
            # parent.  Expected-parent validation follows the task branch's
            # first-parent delivery line so unrelated commits brought in by
            # that merge are not mistaken for the task's first commit.
            commits = git_output(
                repo_root,
                "rev-list",
                "--first-parent",
                "--reverse",
                f"{task['base_sha']}..{task['branch']}",
            ).splitlines()
        except RuntimeError as exc:
            result.error(f"lineage: task {task_id}: {exc}")
            commits = []
        if commits:
            parents = git_output(repo_root, "show", "-s", "--format=%P", commits[0]).split()
            if task["expected_parent_sha"] not in parents:
                result.error(
                    f"lineage: task {task_id} first branch commit {commits[0]} does not have expected parent {task['expected_parent_sha']}"
                )
        for commit in task["result_commits"]:
            if git_commit_exists(repo_root, commit) and not git_is_ancestor(repo_root, commit, branch_tip):
                result.error(f"lineage: task {task_id} result commit {commit} is not on its branch")
        worktree = worktrees.get(canonical_worktree(task["worktree"]))
        if not worktree:
            result.error(f"lineage: task {task_id} worktree is not registered: {task['worktree']}")
        else:
            actual_branch = worktree.get("branch", "").removeprefix("refs/heads/")
            if actual_branch != task["branch"]:
                result.error(f"lineage: task {task_id} worktree branch is {actual_branch}, expected {task['branch']}")

    for session in sessions["sessions"]:
        if session["status"] not in ACTIVE_SESSION_STATES:
            continue
        session_id = session["session_id"]
        if session["role"] in {"emergency_blocker", "security_operator"}:
            if session["branch"] or session["worktree"]:
                result.error(f"lineage: projectless operator {session_id} must remain projectless")
            continue
        if not session["worktree"] or not session["branch"]:
            result.error(f"lineage: active session {session_id} has no branch/worktree")
            continue
        worktree = worktrees.get(canonical_worktree(session["worktree"]))
        if not worktree:
            result.error(f"lineage: active session {session_id} worktree is not registered")
            continue
        actual_branch = worktree.get("branch", "").removeprefix("refs/heads/")
        if actual_branch != session["branch"]:
            result.error(f"lineage: active session {session_id} is on {actual_branch}, expected {session['branch']}")
        if not git_commit_exists(repo_root, session["last_good_sha"]):
            result.error(f"lineage: session {session_id} last_good_sha does not exist")

    for worker in sessions["reported_luna_workers"]:
        if worker["status"] not in ACTIVE_SESSION_STATES or not worker["worktree"]:
            continue
        worker_id = worker["worker_id"]
        if worker["base_sha"] is None or worker["last_good_sha"] is None:
            result.error(f"lineage: active Git worker {worker_id} lacks base/last-good SHA")
            continue
        for label, sha in (("base_sha", worker["base_sha"]), ("last_good_sha", worker["last_good_sha"])):
            if not git_commit_exists(repo_root, sha):
                result.error(f"lineage: worker {worker_id} {label} {sha} does not exist")
        worktree = worktrees.get(canonical_worktree(worker["worktree"]))
        if not worktree:
            result.error(f"lineage: worker {worker_id} worktree is not registered")
            continue
        if not worker["branch"]:
            actual_branch = worktree.get("branch", "").removeprefix("refs/heads/")
            if actual_branch:
                result.error(f"lineage: detached worker {worker_id} unexpectedly has branch {actual_branch}")
            actual_head = worktree.get("HEAD")
            if actual_head != worker["last_good_sha"]:
                result.error(
                    f"lineage: detached worker {worker_id} HEAD {actual_head} "
                    f"does not equal last-good {worker['last_good_sha']}"
                )
            if git_commit_exists(repo_root, worker["base_sha"]) and git_commit_exists(repo_root, worker["last_good_sha"]):
                if not git_is_ancestor(repo_root, worker["base_sha"], worker["last_good_sha"]):
                    result.error(f"lineage: detached worker {worker_id} target does not descend from base")
            continue
        try:
            branch_tip = git_output(repo_root, "rev-parse", f"refs/heads/{worker['branch']}")
        except RuntimeError as exc:
            result.error(f"lineage: worker {worker_id}: {exc}")
            continue
        if not git_is_ancestor(repo_root, worker["base_sha"], branch_tip):
            result.error(f"lineage: worker {worker_id} branch tip does not descend from base")
        if git_commit_exists(repo_root, worker["last_good_sha"]) and not git_is_ancestor(
            repo_root, worker["last_good_sha"], branch_tip
        ):
            result.error(f"lineage: worker {worker_id} last-good commit is not on its branch")
        commits = git_output(repo_root, "rev-list", "--reverse", f"{worker['base_sha']}..{worker['branch']}").splitlines()
        if commits:
            parents = git_output(repo_root, "show", "-s", "--format=%P", commits[0]).split()
            if worker["base_sha"] not in parents:
                result.error(
                    f"lineage: worker {worker_id} first branch commit {commits[0]} "
                    f"does not have base parent {worker['base_sha']}"
                )
        actual_branch = worktree.get("branch", "").removeprefix("refs/heads/")
        if actual_branch != worker["branch"]:
            result.error(f"lineage: worker {worker_id} is on {actual_branch}, expected {worker['branch']}")

    return result


def validate_all(repo_root: Path, include_git: bool) -> tuple[CheckResult, dict[str, Any]]:
    state_dir, schema_dir, _ = orchestration_paths(repo_root)
    result = validate_schemas(state_dir, schema_dir)
    state: dict[str, Any] = {}
    try:
        state = load_all_state(state_dir)
    except ValueError as exc:
        result.error(str(exc))
        return result, state
    result.extend(validate_semantics(repo_root, state))
    if include_git:
        result.extend(validate_git_lineage(repo_root, state))
    return result, state


def print_result(result: CheckResult, label: str) -> int:
    for warning in result.warnings:
        print(f"WARNING: {warning}")
    for error in result.errors:
        print(f"ERROR: {error}")
    if result.errors:
        print(f"{label}: FAILED ({len(result.errors)} error(s), {len(result.warnings)} warning(s))")
        return 1
    print(f"{label}: PASSED ({len(result.warnings)} warning(s))")
    return 0


def markdown_escape(value: Any) -> str:
    return str(value).replace("|", "\\|").replace("\n", " ")


def status_text(state: dict[str, Any]) -> str:
    queue = state["queue"]
    rounds = state["rounds"]
    sessions = state["sessions"]
    acceptance = state["acceptance"]
    blockers = state["blockers"]
    schedule = state["schedule"]
    counts = collections.Counter(task["state"] for task in queue["tasks"])
    classifications = collections.Counter(row["classification"] for row in acceptance["criteria"])
    active_sessions = [row for row in sessions["sessions"] if row["status"] in ACTIVE_SESSION_STATES]
    lines = [
        f"Program base: {queue['program_base_sha']}",
        f"Inventory: {state['inventory']['inventory_status']}",
        "Task states: " + (", ".join(f"{key}={value}" for key, value in sorted(counts.items())) or "none"),
        "Rounds: " + ", ".join(f"{row['round_id']}={row['status']}" for row in rounds["rounds"]),
        f"Active sessions: {len(active_sessions)}",
        f"Reported Luna workers: {len(sessions['reported_luna_workers'])}",
        f"Open external blockers: {sum(row['status'] == 'open' for row in blockers['blockers'])}",
        f"Sol lane capacity: active={schedule['capacity']['active_domain_orchestrators']}, "
        f"cleared={schedule['capacity']['cleared_for_creation']}, "
        f"target={schedule['capacity']['target_active_domain_orchestrators']}, "
        f"monitoring_limit={schedule['capacity']['monitoring_limit']}",
        "Acceptance: " + ", ".join(f"{key}={value}" for key, value in sorted(classifications.items())),
    ]
    return "\n".join(lines)


def dashboard_text(state: dict[str, Any], generated_at: str) -> str:
    queue = state["queue"]
    rounds = state["rounds"]
    sessions = state["sessions"]
    ownership = state["ownership"]
    blockers = state["blockers"]
    acceptance = state["acceptance"]
    inventory = state["inventory"]
    schedule = state["schedule"]
    task_counts = collections.Counter(task["state"] for task in queue["tasks"])
    classification_counts = collections.Counter(row["classification"] for row in acceptance["criteria"])

    lines = [
        "# Tecmo Basketball Finish Status Dashboard",
        "",
        f"Generated from committed JSON at `{generated_at}`. This dashboard reports coordination state only; it is not product QA.",
        "",
        "## Program",
        "",
        f"- Base SHA: `{queue['program_base_sha']}`",
        f"- Inventory: `{inventory['inventory_status']}`",
        f"- Project acceptance: `{acceptance['project_status']}`",
        f"- Open external blockers: `{sum(row['status'] == 'open' for row in blockers['blockers'])}`",
        "- Task states: " + (", ".join(f"`{key}` {value}" for key, value in sorted(task_counts.items())) or "none"),
        "- Fidelity classifications: " + ", ".join(f"`{key}` {value}" for key, value in sorted(classification_counts.items())),
        "",
        "## Sol Orchestration Capacity",
        "",
        f"- Single master authority: `{schedule['policy']['single_master_authority']}`",
        f"- Second-master policy: `{schedule['policy']['second_master_policy']}`",
        f"- Active domain Sols: `{schedule['capacity']['active_domain_orchestrators']}`",
        f"- Cleared for creation: `{schedule['capacity']['cleared_for_creation']}`",
        f"- Target active domain Sols: `{schedule['capacity']['target_active_domain_orchestrators']}`",
        f"- Monitoring limit: `{schedule['capacity']['monitoring_limit']}`",
        "",
        "| Lane | Domain | Readiness | Dependencies | Tasks | Sol | Branch | Next gate |",
        "|---|---|---|---|---|---|---|---|",
    ]
    for lane in schedule["lanes"]:
        lines.append(
            "| " + " | ".join(
                markdown_escape(value)
                for value in (
                    lane["lane_id"], lane["domain"], lane["readiness"], lane["dependency_status"],
                    ", ".join(lane["task_ids"]), lane["orchestrator_session_id"] or "reserved by master",
                    lane["branch"] or "-", lane["next_gate"],
                )
            ) + " |"
        )

    lines.extend([
        "",
        "## Rounds",
        "",
        "| Round | Status | Base | Tasks | Staging | Combined QA | Push |",
        "|---|---|---|---:|---|---|---|",
    ])
    for row in rounds["rounds"]:
        lines.append(
            "| " + " | ".join(
                markdown_escape(value)
                for value in (
                    row["round_id"],
                    row["status"],
                    row["base_sha"][:12],
                    len(row["included_task_ids"]),
                    row["staging_branch"],
                    row["combined_qa"]["status"],
                    row["main_integration"]["push_result"],
                )
            ) + " |"
        )

    lines.extend([
        "",
        "## Queue",
        "",
        "| Priority | Task | Domain | Round | State | Sol session | Branch | Result commits | QA | Merge |",
        "|---:|---|---|---|---|---|---|---:|---|---|",
    ])
    for task in sorted(queue["tasks"], key=lambda row: (-row["priority"], row["task_id"])):
        lines.append(
            "| " + " | ".join(
                markdown_escape(value)
                for value in (
                    task["priority"], task["task_id"], task["domain"], task["round_id"], task["state"],
                    task["sol_orchestrator_session_id"] or "-", task["branch"] or "-", len(task["result_commits"]),
                    task["qa"]["status"], task["merge"]["status"],
                )
            ) + " |"
        )

    lines.extend([
        "",
        "## Active Sessions",
        "",
        "| Session | Role | Model/thinking | Status | Pin | Tasks | Branch | Worktree | Last good |",
        "|---|---|---|---|---|---|---|---|---|",
    ])
    for session in sessions["sessions"]:
        if session["status"] not in ACTIVE_SESSION_STATES:
            continue
        lines.append(
            "| " + " | ".join(
                markdown_escape(value)
                for value in (
                    session["session_id"], session["role"], f"{session['model']}/{session['thinking']}",
                    session["status"], session["pin_state"], ", ".join(session["task_ids"]),
                    session["branch"] or "-", session["worktree"] or "-", session["last_good_sha"][:12],
                )
            ) + " |"
        )

    lines.extend([
        "",
        "## Active Ownership",
        "",
        "| Claim | Task | Round | Mode | Writable globs | Concurrency group |",
        "|---|---|---|---|---|---|",
    ])
    for claim in ownership["claims"]:
        if claim["active"]:
            lines.append(
                "| " + " | ".join(
                    markdown_escape(value)
                    for value in (
                        claim["claim_id"], claim["task_id"], claim["round_id"], claim["mode"],
                        "<br>".join(claim["writable_globs"]), claim["concurrency_group"],
                    )
                ) + " |"
            )

    lines.extend([
        "",
        "## External Blockers",
        "",
    ])
    if blockers["blockers"]:
        lines.extend(["| Blocker | Task | Category | Status | Required action |", "|---|---|---|---|---|"])
        for blocker in blockers["blockers"]:
            lines.append(
                "| " + " | ".join(
                    markdown_escape(value)
                    for value in (
                        blocker["blocker_id"], blocker["task_id"], blocker["category"], blocker["status"],
                        blocker["required_user_action"],
                    )
                ) + " |"
            )
    else:
        lines.append("No external blockers are recorded.")

    lines.extend([
        "",
        "## Completion Matrix",
        "",
        "| Criterion | Domain | Classification | Status | Tasks | Evidence |",
        "|---|---|---|---|---|---|",
    ])
    for criterion in acceptance["criteria"]:
        lines.append(
            "| " + " | ".join(
                markdown_escape(value)
                for value in (
                    f"{criterion['criterion_id']}: {criterion['title']}", criterion["domain"],
                    criterion["classification"], criterion["status"], ", ".join(criterion["task_ids"]),
                    ", ".join(criterion["evidence_ids"]),
                )
            ) + " |"
        )

    lines.extend([
        "",
        "## Recovery",
        "",
        "Read `MASTER_PLAN.md`, validate all state, verify Git lineage, then contact only active Sol orchestrators registered above.",
        "",
    ])
    return "\n".join(lines)


def command_validate(args: argparse.Namespace) -> int:
    result, _ = validate_all(args.repo_root, args.git)
    return print_result(result, "control-plane validation")


def command_status(args: argparse.Namespace) -> int:
    result, state = validate_all(args.repo_root, False)
    if result.errors:
        return print_result(result, "status preflight")
    print(status_text(state))
    return 0


def command_dashboard(args: argparse.Namespace) -> int:
    result, state = validate_all(args.repo_root, args.git)
    if result.errors:
        return print_result(result, "dashboard preflight")
    _, _, dashboard_path = orchestration_paths(args.repo_root)
    generated_at = args.generated_at or state["queue"]["updated_at"]
    content = dashboard_text(state, generated_at).rstrip() + "\n"
    if args.check:
        if not dashboard_path.is_file() or dashboard_path.read_text(encoding="utf-8") != content:
            print(f"ERROR: dashboard is stale: {dashboard_path}")
            return 1
        print(f"dashboard check: PASSED ({dashboard_path})")
        return 0
    dashboard_path.write_text(content, encoding="utf-8", newline="\n")
    print(f"wrote dashboard: {dashboard_path}")
    return 0


def command_lineage(args: argparse.Namespace) -> int:
    state_dir, _, _ = orchestration_paths(args.repo_root)
    try:
        state = load_all_state(state_dir)
    except ValueError as exc:
        print(f"ERROR: {exc}")
        return 1
    return print_result(validate_git_lineage(args.repo_root, state), "Git lineage validation")


def command_transition(args: argparse.Namespace) -> int:
    state_dir, _, _ = orchestration_paths(args.repo_root)
    queue_path = state_dir / "queue.json"
    queue = load_json(queue_path)
    machine = load_json(state_dir / "state-machine.json")
    sessions = load_json(state_dir / "sessions.json")
    session_ids = {row["session_id"] for row in sessions["sessions"]}
    if args.actor_session_id not in session_ids:
        print(f"ERROR: unknown actor session {args.actor_session_id}")
        return 1
    task = next((row for row in queue["tasks"] if row["task_id"] == args.task_id), None)
    if not task:
        print(f"ERROR: unknown task {args.task_id}")
        return 1
    allowed = {row["from"]: set(row["to"]) for row in machine["transitions"]}
    current = task["state"]
    if args.to_state not in allowed.get(current, set()):
        print(f"ERROR: forbidden transition {current} -> {args.to_state}")
        return 1
    if args.to_state == "replaced" and not args.replacement_task_id:
        print("ERROR: replaced transition requires --replacement-task-id")
        return 1
    timestamp = args.timestamp or utc_now()
    task["audit"].append(
        {
            "timestamp": timestamp,
            "actor_session_id": args.actor_session_id,
            "from_state": current,
            "to_state": args.to_state,
            "reason": args.reason.strip(),
        }
    )
    task["state"] = args.to_state
    task["updated_at"] = timestamp
    if args.replacement_task_id:
        task["replacement_task_id"] = args.replacement_task_id
    queue["updated_at"] = timestamp
    if args.dry_run:
        print(f"dry run: {args.task_id} {current} -> {args.to_state} at {timestamp}")
        return 0
    write_json_atomic(queue_path, queue)
    print(f"transitioned {args.task_id}: {current} -> {args.to_state}")
    return 0


def command_self_test(args: argparse.Namespace) -> int:
    failures: list[str] = []

    def expect(condition: bool, label: str) -> None:
        if not condition:
            failures.append(label)

    expect(matches_glob("src/game.c", "src/**"), "recursive glob matches file")
    expect(matches_glob("src/sub/game.c", "src/**/*.c"), "recursive extension glob matches nested file")
    expect(not matches_glob("docs/game.md", "src/**"), "disjoint glob does not match")
    expect(possible_pattern_overlap("src/**", "src/gameplay/**"), "ancestor globs overlap")
    expect(not possible_pattern_overlap("src/audio/**", "src/gameplay/**"), "disjoint prefixes do not overlap")
    expect(find_duplicates(["a", "b", "a"]) == ["a"], "duplicate detector")

    prior_task = {
        "task_id": "A", "round_id": "R1", "sol_orchestrator_session_id": "S1",
        "state": "sol_accepted", "dependencies": [],
    }
    next_task = {
        "task_id": "B", "round_id": "R1", "sol_orchestrator_session_id": "S1",
        "state": "in_progress", "dependencies": ["A"],
    }
    expect(tasks_may_share_sequential_context(prior_task, next_task), "same-Sol sequential context reuse")
    delivery_task = copy.deepcopy(prior_task)
    delivery_task["round_id"] = "R1A"
    delivery_task["state"] = "ready_for_round_staging"
    expect(
        tasks_may_share_sequential_context(delivery_task, next_task),
        "frozen delivery-subround handoff context reuse",
    )
    unfrozen_cross_round = copy.deepcopy(delivery_task)
    unfrozen_cross_round["state"] = "sol_accepted"
    expect(
        not tasks_may_share_sequential_context(unfrozen_cross_round, next_task),
        "unfrozen cross-round context rejection",
    )
    concurrent_task = copy.deepcopy(prior_task)
    concurrent_task["state"] = "sol_review"
    expect(
        not tasks_may_share_sequential_context(concurrent_task, next_task),
        "concurrent writable context rejection",
    )
    unordered_task = copy.deepcopy(next_task)
    unordered_task["dependencies"] = []
    expect(
        not tasks_may_share_sequential_context(prior_task, unordered_task),
        "unordered context reuse rejection",
    )
    other_sol_task = copy.deepcopy(next_task)
    other_sol_task["sol_orchestrator_session_id"] = "S2"
    expect(
        not tasks_may_share_sequential_context(prior_task, other_sol_task),
        "cross-Sol context reuse rejection",
    )

    sample_tasks = {
        "A": {"dependencies": ["B"]},
        "B": {"dependencies": ["A"]},
    }
    expect(bool(detect_dependency_cycles(sample_tasks)), "dependency cycle detector")
    expect(set(STATE_SCHEMA_MAP) == {
        "acceptance.json", "blockers.json", "evidence.json", "inventory.json", "ownership.json",
        "queue.json", "rounds.json", "schedule.json", "sessions.json", "state-machine.json"
    }, "state/schema registry")
    with tempfile.TemporaryDirectory(prefix="tecmo-doc-root-selftest-") as temp_root:
        fixture_root = Path(temp_root)
        master_root = fixture_root / "master"
        task_root = fixture_root / "domain-worktree"
        registered_docs = task_root / "docs" / "finish-tasks" / "sample"
        registered_docs.mkdir(parents=True)
        docs_fixture = {
            "worktree": str(task_root),
            "task_docs_path": "docs/finish-tasks/sample",
        }
        candidates = task_docs_candidates(master_root, docs_fixture)
        expect(candidates[0] == registered_docs and candidates[0].is_dir(), "registered-worktree task docs")
        expect(candidates[-1] == master_root / "docs" / "finish-tasks" / "sample", "master-root docs fallback")

    state_dir, schema_dir, _ = orchestration_paths(args.repo_root)
    try:
        actual_state = load_all_state(state_dir)

        duplicate_state = copy.deepcopy(actual_state)
        duplicate_state["queue"]["tasks"].append(copy.deepcopy(duplicate_state["queue"]["tasks"][0]))
        duplicate_result = validate_semantics(args.repo_root, duplicate_state)
        expect(any("duplicate task id" in message for message in duplicate_result.errors), "semantic duplicate task rejection")

        audit_state = copy.deepcopy(actual_state)
        audit_state["queue"]["tasks"][0]["audit"][-1]["from_state"] = "backlog"
        audit_result = validate_semantics(args.repo_root, audit_state)
        expect(any("does not continue prior state" in message for message in audit_result.errors), "audit-chain rejection")

        dependency_state = copy.deepcopy(actual_state)
        dependency_task = next(
            row for row in dependency_state["queue"]["tasks"] if row["task_id"] == "R0A-ADOPT-CPU-TIP"
        )
        dependency_task["state"] = "backlog"
        dependency_result = validate_semantics(args.repo_root, dependency_state)
        expect(
            any("unsatisfied dependency R0A-ADOPT-CPU-TIP" in message for message in dependency_result.errors),
            "active-task dependency gate",
        )

        worker_collision_state = copy.deepcopy(actual_state)
        collision_worker = worker_collision_state["sessions"]["reported_luna_workers"][0]
        collision_worker.update(
            {
                "status": "active",
                "pin_state": "pinned",
                "branch": "codex/master-finish-orchestration",
                "worktree": str(args.repo_root),
                "base_sha": worker_collision_state["queue"]["program_base_sha"],
                "last_good_sha": worker_collision_state["queue"]["program_base_sha"],
            }
        )
        worker_collision_result = validate_semantics(args.repo_root, worker_collision_state)
        expect(
            any("shares branch" in message for message in worker_collision_result.errors),
            "reported-worker active-context collision rejection",
        )

        overlap_claims = copy.deepcopy(actual_state["ownership"])
        first_claim = copy.deepcopy(overlap_claims["claims"][0])
        first_claim.update(
            {
                "claim_id": "SELF-TEST-OVERLAP-A",
                "task_id": "SELF-TEST-TASK-A",
                "round_id": "SELF-TEST-ROUND",
                "session_id": "SELF-TEST-SESSION-A",
                "writable_globs": ["src/**"],
                "planned_paths": ["src/tecmo_gameplay_scene.c"],
                "active": True,
                "released_at": None,
            }
        )
        second_claim = copy.deepcopy(first_claim)
        second_claim.update(
            {
                "claim_id": "SELF-TEST-OVERLAP-B",
                "task_id": "SELF-TEST-TASK-B",
                "session_id": "SELF-TEST-SESSION-B",
                "writable_globs": ["src/tecmo_gameplay_scene*.c"],
            }
        )
        overlap_claims["claims"] = [first_claim, second_claim]
        overlap_claims["shared_boundaries"] = []
        expect(bool(ownership_overlaps(args.repo_root, overlap_claims)), "ownership-overlap rejection")

        invalid_queue = copy.deepcopy(actual_state["queue"])
        invalid_queue["tasks"][0]["priority"] = 101
        queue_schema = load_json(schema_dir / STATE_SCHEMA_MAP["queue.json"])
        queue_errors = list(
            Draft202012Validator(queue_schema, format_checker=FormatChecker()).iter_errors(invalid_queue)
        )
        expect(bool(queue_errors), "JSON Schema rejection")
    except (ValueError, OSError) as exc:
        failures.append(f"fixture setup: {exc}")

    if failures:
        for failure in failures:
            print(f"ERROR: self-test failed: {failure}")
        print(f"control-plane self-test: FAILED ({len(failures)} failure(s))")
        return 1
    print("control-plane self-test: PASSED")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root (default: inferred from this script)",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    validate = subparsers.add_parser("validate", help="Validate schemas and semantic coordination invariants")
    validate.add_argument("--git", action="store_true", help="Also validate live Git branch/worktree/commit lineage")
    validate.set_defaults(func=command_validate)

    status = subparsers.add_parser("status", help="Show compact queue/round status")
    status.set_defaults(func=command_status)

    dashboard = subparsers.add_parser("dashboard", help="Generate or check the Markdown dashboard")
    dashboard.add_argument("--git", action="store_true", help="Require live Git lineage validation first")
    dashboard.add_argument("--check", action="store_true", help="Fail if the committed dashboard is stale")
    dashboard.add_argument("--generated-at", help="Stable RFC3339 timestamp to embed; defaults to queue.updated_at")
    dashboard.set_defaults(func=command_dashboard)

    lineage = subparsers.add_parser("lineage", help="Verify live Git base/parent/branch/worktree/result lineage")
    lineage.set_defaults(func=command_lineage)

    transition = subparsers.add_parser("transition", help="Apply one permitted audited task transition")
    transition.add_argument("--task-id", required=True)
    transition.add_argument("--to-state", required=True)
    transition.add_argument("--actor-session-id", required=True)
    transition.add_argument("--reason", required=True)
    transition.add_argument("--replacement-task-id")
    transition.add_argument("--timestamp", help="RFC3339 UTC timestamp; defaults to now")
    transition.add_argument("--dry-run", action="store_true")
    transition.set_defaults(func=command_transition)

    self_test = subparsers.add_parser("self-test", help="Exercise validator primitives and failure detection")
    self_test.set_defaults(func=command_self_test)
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    args.repo_root = args.repo_root.resolve()
    return int(args.func(args))


if __name__ == "__main__":
    sys.exit(main())
