#!/usr/bin/env python3
"""Check that repeated solve rounds cannot reuse application state."""

from pathlib import Path
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
    bodies = loop_bodies(path.read_text())
    if len(bodies) != expected_loops:
        raise AssertionError(
            f"{path}: expected {expected_loops} trial loops, got {len(bodies)}"
        )
    for index, body in enumerate(bodies):
        for marker in (create, destroy, "memset"):
            if marker not in body:
                raise AssertionError(
                    f"{path}: loop {index} is missing required marker {marker}"
                )


def check_configs():
    for path in (ROOT / "problems").glob("problem*/config/*"):
        if not path.is_file():
            continue
        text = path.read_text(errors="replace")
        if "XK_DIAG_TRIALS=10" in text:
            raise AssertionError(f"{path}: nested ten-trial diagnostic is forbidden")

    t08 = (ROOT / "problems/problem08/config/runtime.env").read_text()
    for setting in (
        "XI_FILL_CACHE=1",
        "XI_PATTERN_CACHE=1",
        "XI_REUSE_WORKSPACE=1",
    ):
        if setting in t08:
            raise AssertionError(f"T08 keeps forbidden cross-round state: {setting}")


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


def main():
    apps = ROOT / "apps"
    cpu_drivers = (
        "iterative_solver.c",
        "direct_solver.c",
        "iterative_solver_multi.c",
        "direct_solver_multi.c",
        "MPI_iterative_solver.c",
        "MPI_direct_solver.c",
    )
    for name in cpu_drivers:
        check_driver(
            apps / name, "SC26_SOLVER_CREATE", "SC26_SOLVER_DESTROY", 2
        )

    check_driver(
        apps / "MPI_GPU_iterative_solver.c",
        "sc26_gpu_iterative_solver_create",
        "sc26_gpu_iterative_solver_destroy",
        1,
    )
    check_driver(
        apps / "MPI_GPU_direct_solver.c",
        "sc26_gpu_direct_solver_create",
        "sc26_gpu_direct_solver_destroy",
        1,
    )
    check_configs()
    check_gitee_layout()
    print("round isolation checks passed")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
