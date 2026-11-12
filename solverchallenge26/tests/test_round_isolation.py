#!/usr/bin/env python3
"""Check that repeated rounds rebuild solver state from immutable inputs."""

from pathlib import Path
import json
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def loop_bodies(text):
    bodies = []
    cursor = 0
    while True:
        start = text.find("for (int trial", cursor)
        if start < 0:
            return bodies
        brace = text.find("{", start)
        if brace < 0:
            raise AssertionError("trial loop has no body")
        depth = 0
        end = None
        for index in range(brace, len(text)):
            if text[index] == "{":
                depth += 1
            elif text[index] == "}":
                depth -= 1
                if depth == 0:
                    end = index + 1
                    break
        if end is None:
            raise AssertionError("unterminated trial loop")
        bodies.append(text[start:end])
        cursor = end


def check_driver(path, create, destroy, expected_loops):
    text = path.read_text()
    bodies = loop_bodies(text)
    if len(bodies) != expected_loops:
        raise AssertionError(
            f"{path}: expected {expected_loops} trial loops, got {len(bodies)}"
        )
    for index, body in enumerate(bodies):
        if "memset" not in body:
            raise AssertionError(f"{path}: loop {index} does not restore x=0")
        reset_at = body.find("SC26_SOLVER_RESET")
        timer_markers = [
            position
            for marker in ("GetCurrentTime()", "sc26_multi_seconds()")
            if (position := body.find(marker)) >= 0
        ]
        if reset_at < 0:
            raise AssertionError(
                f"{path}: loop {index} does not reset device solution state"
            )
        if not timer_markers:
            raise AssertionError(f"{path}: loop {index} has no round timer")
        if reset_at > min(timer_markers):
            raise AssertionError(
                f"{path}: loop {index} resets solution after timing starts"
            )
        if "analyse" not in body and "preprocess" not in body:
            raise AssertionError(f"{path}: loop {index} has no timed setup phase")
        for marker in (create, destroy):
            if marker in body:
                raise AssertionError(
                    f"{path}: loop {index} creates or destroys solver state"
                )
    if text.count(create) != expected_loops:
        raise AssertionError(f"{path}: solver must be created once per branch")
    if text.count(destroy) != expected_loops:
        raise AssertionError(f"{path}: solver must be destroyed once per branch")


def check_configs():
    for path in (ROOT / "problems").glob("problem*/config/*"):
        if not path.is_file():
            continue
        text = path.read_text(errors="replace")
        if "XK_DIAG_TRIALS=10" in text:
            raise AssertionError(f"{path}: nested ten-trial diagnostic is forbidden")
        if path.name.startswith("amg_") and path.suffix == ".json":
            config = json.loads(text)
            reuse = (
                config.get("setup", {}).get("reuse_structure", 0)
                if isinstance(config, dict)
                else 0
            )
            if reuse not in (0, False):
                raise AssertionError(
                    f"{path}: LinglongAMG must rebuild structure every round"
                )

    for path in (ROOT / "problems").glob("problem*/config/runtime*.env"):
        text = path.read_text(errors="replace")
        for setting in (
            "XI_FILL_CACHE=1",
            "XI_PATTERN_CACHE=1",
            "XI_REUSE_WORKSPACE=1",
            "XK_T7_REUSE_ARENA=1",
        ):
            if setting in text:
                raise AssertionError(
                    f"{path}: forbidden cross-round state: {setting}"
                )
    for problem in ("09", "10"):
        config = json.loads(
            (ROOT / f"problems/problem{problem}/config/solver.json").read_text()
        )
        if config["pangu_lu"]["reuse"] != "L0":
            raise AssertionError(
                f"T{problem}: PanguLU must disable analysis/factor reuse"
            )


def check_gitee_layout():
    required = (
        "CMakeLists.txt",
        "CMakePresets.json",
        "Makefile",
        "README.md",
        "apps/iterative_solver.c",
        "apps/direct_solver.c",
        "apps/MPI_iterative_solver.c",
        "apps/MPI_direct_solver.c",
        "apps/MPI_GPU_iterative_solver.c",
        "apps/MPI_GPU_direct_solver.c",
        "include/solverchallenge26/solver_api.h",
        "problems/README.md",
    )
    for relative in required:
        if not (ROOT / relative).is_file():
            raise AssertionError(f"missing Gitee-compatible file: {relative}")
    for problem in range(1, 11):
        problem_dir = ROOT / "problems" / f"problem{problem:02d}"
        for name in (
            "CMakeLists.txt",
            "solver.c",
            "solver.h",
            "gpu_direct_solver.cu",
            "gpu_iterative_solver.cu",
        ):
            if not (problem_dir / name).is_file():
                raise AssertionError(
                    f"missing Gitee-compatible problem slot: {problem_dir.name}/{name}"
                )


def check_run_script():
    text = (ROOT / "scripts/run_problem.sh").read_text()
    assignments = re.findall(
        r"(?m)^export SC26_PANGULU_REUSE=(\S+)$",
        text,
    )
    if assignments != ["L0"]:
        raise AssertionError(
            "run_problem.sh must set SC26_PANGULU_REUSE=L0 exactly once"
        )


def check_product_reset():
    xianzhi = (ROOT / "src/product/xianzhi_solver.c").read_text()
    solve_start = xianzhi.index("int sc26_xianzhi_solver_solve(")
    solve_end = xianzhi.index("\n}\n", solve_start) + 3
    solve_body = xianzhi[solve_start:solve_end]
    if "device_zero(" in solve_body:
        raise AssertionError(
            "XianzhiKrylov solve must not reset the solution inside its timer"
        )
    if "sc26_xianzhi_solver_reset(" not in xianzhi:
        raise AssertionError("XianzhiKrylov reset entry point is missing")

    pangulu = (ROOT / "src/product/pangulu_direct_solver.cpp").read_text()
    if "sc26_pangulu_direct_solver_reset(" not in pangulu:
        raise AssertionError("PanguLU reset entry point is missing")


def main():
    apps = ROOT / "apps"
    cpu_drivers = (
        "iterative_solver.c",
        "direct_solver.c",
        "iterative_solver_multi.c",
        "direct_solver_multi.c",
    )
    for name in cpu_drivers:
        check_driver(
            apps / name, "SC26_SOLVER_CREATE", "SC26_SOLVER_DESTROY", 2
        )

    check_configs()
    check_run_script()
    check_product_reset()
    check_gitee_layout()
    print("round isolation checks passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
