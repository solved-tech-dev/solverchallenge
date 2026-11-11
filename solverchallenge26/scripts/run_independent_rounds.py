#!/usr/bin/env python3
"""Run ten solves without sharing a process or writable working directory."""

import os
from pathlib import Path
import subprocess
import sys


def restore_verification_affinity(pid, allowed):
    """Undo vendor thread binding only after the solver timer has stopped."""
    for thread in Path(f"/proc/{pid}/task").glob("*"):
        try:
            os.sched_setaffinity(int(thread.name), allowed)
        except (OSError, ValueError):
            # Exiting worker threads need no further adjustment.
            continue


def run_rounds(command, output):
    allowed = os.sched_getaffinity(0) if hasattr(os, "sched_getaffinity") else None
    for trial in range(10):
        directory = output / "rounds" / f"round-{trial:02d}"
        directory.mkdir(parents=True, exist_ok=False)
        temporary = directory / "tmp"
        temporary.mkdir()
        cache = directory / "cache"
        cache.mkdir()
        environment = os.environ.copy()
        environment["SC26_TRIAL_INDEX"] = str(trial)
        environment["TMPDIR"] = str(temporary)
        environment["XDG_CACHE_HOME"] = str(cache)

        print(f"ROUND_BEGIN trial={trial}", flush=True)
        with (directory / "run.log").open("wb") as log:
            process = subprocess.Popen(
                command,
                cwd=directory,
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )
            for line in process.stdout:
                log.write(line)
                log.flush()
                # All supported drivers emit this line after timing and
                # destroying the solver, immediately before CPU verification.
                if allowed and line.startswith(b"Total memory usage:"):
                    restore_verification_affinity(process.pid, allowed)
                sys.stdout.buffer.write(line)
                sys.stdout.buffer.flush()
            status = process.wait()
        print(f"ROUND_END trial={trial} exit_code={status}", flush=True)
        if status:
            return status if status > 0 else 128 - status
    return 0


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: run_independent_rounds.py COMMAND [ARG ...]")
    raise SystemExit(run_rounds(sys.argv[1:], Path.cwd()))


if __name__ == "__main__":
    main()
