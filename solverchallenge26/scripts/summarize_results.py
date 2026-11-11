#!/usr/bin/env python3
"""Create a concise performance table from SolverChallenge26 logs."""

import argparse
import csv
import json
import os
from pathlib import Path
import re
import statistics
import sys


NUMBER = r"([0-9.eE+\-]+)"
WARMUP_ROUNDS = 5
MEASURED_ROUNDS = 5
LAST_TRIAL = WARMUP_ROUNDS + MEASURED_ROUNDS - 1

OUTPUT_FIELDS = (
    "case",
    "status",
    "time_ms",
    "iterations",
    "max_relative_l2",
    "max_absolute_max",
)


def compact_iterations(values):
    """Return one integer when all measured rounds used the same count."""
    if not values:
        return None
    if len(set(values)) == 1:
        return values[0]
    return ",".join(str(value) for value in values)


def summarize_log(path):
    """Read one solver log and return one user-facing result row."""
    text = path.read_text(errors="replace")
    case_name = path.stem
    if case_name == "run":
        case_name = os.environ.get("SC26_RESULT_CASE", path.parent.name)
    result = {
        "case": case_name,
        "status": "INCOMPLETE",
    }
    rounds = []
    trial_pattern = r"TRIAL trial=(\d+) raw_seconds=" + NUMBER
    matches = list(re.finditer(trial_pattern, text))

    for index, match in enumerate(matches):
        tail_end = matches[index + 1].start() if index + 1 < len(matches) else len(text)
        tail = text[match.end():tail_end]
        row = {
            "trial": int(match[1]),
            "raw_seconds": float(match[2]),
        }

        for key in (
            "prepare_solve_seconds",
            "excluded_input_transfer_seconds",
            "excluded_format_conversion_seconds",
        ):
            found = re.search(r"\b" + key + "=" + NUMBER, tail)
            if found:
                row[key] = float(found[1])

        found = re.search(
            r"XianzhiKrylov status=\d+ converged=\d+ iterations=(\d+)",
            tail,
        )
        if found:
            row["iterations"] = int(found[1])

        # The application timer already starts after input loading and emits
        # raw_seconds over the competition solve phases.  The excluded_* fields
        # are diagnostics; on reused T04/T07 paths they can describe work that
        # was not repeated and must not be subtracted from every measured round.
        row["time_seconds"] = row["raw_seconds"]
        rounds.append(row)

    measured = [
        row
        for row in rounds
        if WARMUP_ROUNDS <= row["trial"] <= LAST_TRIAL
    ]

    if len(measured) == MEASURED_ROUNDS:
        result["time_ms"] = round(
            1000.0 * statistics.mean(row["time_seconds"] for row in measured),
            6,
        )
        result["iterations"] = compact_iterations(
            [row["iterations"] for row in measured if "iterations" in row]
        )
    else:
        # Compatibility with logs produced before per-round timing was added.
        found = re.search(r"COMPETITION_TIME_SECONDS=" + NUMBER, text)
        if found:
            result["time_ms"] = round(1000.0 * float(found[1]), 6)
        else:
            found = re.search(r"(?m)^time_ms=" + NUMBER, text)
            if found:
                result["time_ms"] = round(float(found[1]), 6)

    rhs_checks = re.findall(
        r"RESIDUAL rhs=(\d+) relative_l2="
        + NUMBER
        + r" absolute_max="
        + NUMBER
        + r" (PASS|FAIL)",
        text,
    )
    final_check = re.search(
        r"VERIFICATION relative_l2="
        + NUMBER
        + r" threshold="
        + NUMBER
        + r" (PASS|FAIL)",
        text,
    )

    if rhs_checks:
        result["max_relative_l2"] = max(float(row[1]) for row in rhs_checks)
        result["max_absolute_max"] = max(float(row[2]) for row in rhs_checks)
        if len(rhs_checks) == 10 and all(row[3] == "PASS" for row in rhs_checks):
            result["status"] = "PASS"
        else:
            result["status"] = "FAIL"
    elif final_check:
        result["max_relative_l2"] = float(final_check[1])
        result["status"] = final_check[3]

    error_markers = (
        "solver creation failed",
        "numeric preparation failed",
        "segmentation fault",
        "core dumped",
    )
    if any(marker in text.lower() for marker in error_markers):
        result["status"] = "FAIL"

    return result


def collect_logs(path):
    """Resolve a run directory, one log, or several logs."""
    if path.is_dir():
        run_logs = path / "run-logs"
        if not run_logs.is_dir():
            raise ValueError(f"run-log directory not found: {run_logs}")
        return sorted(run_logs.glob("*.log")), path
    if path.is_file():
        return [path], path.parent
    raise ValueError(f"path not found: {path}")


def write_outputs(rows, output_dir):
    """Write both a machine-readable JSON file and a small TSV table."""
    output_dir.mkdir(parents=True, exist_ok=True)
    performance_json = output_dir / "performance.json"
    performance_tsv = output_dir / "performance.tsv"

    performance_json.write_text(json.dumps(rows, indent=2) + "\n")
    with performance_tsv.open("w", newline="") as stream:
        writer = csv.DictWriter(
            stream,
            fieldnames=OUTPUT_FIELDS,
            delimiter="\t",
            extrasaction="ignore",
        )
        writer.writeheader()
        writer.writerows(rows)
    return performance_tsv


def parse_args(argv):
    parser = argparse.ArgumentParser(
        description="Create performance.tsv and performance.json from solver logs."
    )
    parser.add_argument(
        "paths",
        nargs="+",
        metavar="RUN_DIR_OR_LOG",
        help="run directory, a single run.log, or several logs from one output directory",
    )
    return parser.parse_args(argv)


def main(argv):
    args = parse_args(argv[1:])

    rows = []
    output_dir = None
    for raw_path in args.paths:
        logs, candidate_output = collect_logs(Path(raw_path))
        if output_dir is None:
            output_dir = candidate_output
        elif candidate_output != output_dir:
            raise SystemExit("all log files must be in the same output directory")
        rows.extend(summarize_log(log) for log in logs)

    performance_tsv = write_outputs(rows, output_dir)
    print(performance_tsv)
    return 0 if rows and all(row["status"] == "PASS" for row in rows) else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
