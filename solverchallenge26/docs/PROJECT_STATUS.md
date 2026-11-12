# SolverChallenge26 Project Status

This file is the platform-neutral entry point for continuing the work in a
new session. It intentionally uses repository-relative paths and public URLs
only. Do not store conversation exports, local home-directory paths, NAS paths,
account names, private links, product licenses, or competition matrices here.

## Canonical source

- Repository: <https://github.com/solved-tech-dev/solverchallenge>
- Project directory: `solverchallenge26/`
- Build and run instructions: [`README.md`](../README.md)
- Problem-specific settings: `problems/problem01/` through
  `problems/problem10/`

## What is included

The repository includes the ten solver programs, product adapters, runtime
settings, build scripts, result summaries, and the tests derived from the
competition template.

The repository does not include product libraries, licenses, matrices, or
right-hand-side files. Obtain those from the product store and competition
organizer, then follow `README.md` to place them under `local/`. Keep those
files outside Git.

## Handoff rules

1. Start from a fresh clone of `main` and record the exact commit.
2. Read `README.md`, then the relevant `problems/problemNN/README.md`.
3. Keep packages, licenses, matrices, logs, and generated results outside the
   tracked source tree or under ignored paths.
4. Record externally supplied inputs by version and checksum. Never put a
   license key in a document, commit, or issue.
5. Before reporting a performance result, record the library versions,
   compiler and GPU runtime, matrix identity, command, iterations, residual,
   and raw result directory.
6. Store reusable handoff information in repository documentation. Do not
   depend on a chat transcript, a local desktop path, a private attachment, or
   a path that is valid on only one machine.

## Competition timing checklist

The current programs follow the SolverChallenge26 phase structure:

- iterative methods: `ANALYSIS -> NUMERIC_PREPARE -> SOLVE`
- direct methods: `ANALYSIS -> NUMERIC_FACTORISATION -> SOLVE`
- ten rounds per process: five warm-up rounds followed by five measured rounds
- reported value: mean of rounds 6 through 10

Input reading, matrix conversion, CPU/GPU transfer, object creation, solution
download, and independent residual verification are outside the measured
intervals. Every round must rebuild analysis, numeric preparation or
factorization, and the solution state; no computed state may be carried from
one round to the next.

The standard drivers call `SC26_SOLVER_RESET` after restoring the host-side
zero vector and before starting the round timer. The XianzhiKrylov and PanguLU
adapters restore their device solution buffers in that callback. Do not move
this reset into `ANALYSIS`, numeric preparation, factorization, or `SOLVE`.

Before accepting a result as competition-compliant, verify in the actual
source and logs that:

- `SC26_SOLVER_RESET` completes before the round timer starts;
- matrix A and the RHS files are loaded once only as immutable inputs;
- every measured round starts with a fresh `ANALYSIS`;
- no symbolic analysis, numeric factors, hierarchy, pattern/fill cache, or
  previous solution survives across rounds;
- direct-solver solution buffers are explicitly reset before each timed
  round;
- residual verification runs after `SOLVE` is synchronized and outside the
  timer.

## Current continuation point

The next authenticated ten-problem check is a manual acceptance test. Prepare:

- a fresh GPU allocation on the competition cluster;
- current PanguLU, LinglongAMG, and XianzhiKrylov packages for the allocation;
- valid licenses bound to that GPU;
- the competition data root.

Then follow the commands in `README.md`. Report the exact commit, package
versions and hashes, GPU runtime, command, per-problem phase times, iteration
counts, real residuals, and any failed case logs.

Do not infer performance from this status file. The source, logs, and raw
result files are authoritative.
