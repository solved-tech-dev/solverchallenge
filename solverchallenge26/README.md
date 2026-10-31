# SolverChallenge26 Problem Solvers

This repository contains ready-to-build solver programs for all ten
SolverChallenge26 problems.

The repository does not contain product libraries, licenses, matrices, or
right-hand sides. You can download the libraries and licenses from
[https://store.solved-tech.com/](https://store.solved-tech.com/).

## What you need

You need:

- a Linux computer or an interactive GPU allocation from your cluster;
- a C and C++ compiler;
- CMake 3.21 or newer;
- the vendor runtime required by the selected product package;
- the product packages and licenses for the problems you want to run;
- the competition matrix and right-hand-side files.

Start an interactive GPU allocation by using the method provided by your
cluster.

The required products are:

| Problems | Packages and licenses |
|---|---|
| T01, T02, T05 | LinglongAMG and XianzhiKrylov |
| T03, T06 | PanguLU and XianzhiKrylov |
| T07, T08 | XianzhiKrylov |
| T09, T10 | PanguLU |

## Repository layout

```text
.
├── apps/                       common test-program entry points
├── include/                    official SolverChallenge26 interfaces
├── src/                        input readers, residual checks, product adapters
├── problems/problem01..10/     problem-specific solver settings
├── tests/                      template tests
├── configs/local.env.example   optional custom-path template
├── scripts/                    commands used in this guide
├── local/                      local packages and licenses; not committed
├── build/                      build output; created automatically
└── results/                    run output; created automatically
```

The `apps/`, `include/`, `src/`, `problems/`, and `tests/` directories follow
the public SolverChallenge26 template.

## Setup and Run

### Step 1: Get the project

If Git is available, run:

```bash
git clone https://github.com/solved-tech-dev/solverchallenge.git
cd solverchallenge/solverchallenge26
```

Run all remaining commands from the project directory. If using an archive,
extract it and enter the extracted
`solverchallenge-main/solverchallenge26` directory.

Make the helper scripts executable:

```bash
chmod +x scripts/*.sh
```

### Step 2: Check the basic tools

Run:

```bash
cmake --version
c++ --version
```

If a command is missing, load the corresponding environment module before
continuing.

### Step 3: Create the local folders

Packages and licenses are deliberately kept outside Git. Run:

```bash
mkdir -p local/downloads \
         local/packages/pangulu \
         local/packages/linglongamg \
         local/packages/xianzhikrylov \
         local/licenses
```

### Step 4: Put the downloaded packages in `local/downloads`

Copy one package archive for each required product into `local/downloads`:

```text
local/downloads/pangulu-*.tar.gz
local/downloads/linglongamg-*.tar.gz
local/downloads/xianzhikrylov-*.tar.gz
```

Use packages built for the target GPU and its installed vendor runtime.

### Step 5: Extract the packages

```bash
tar -xzf local/downloads/pangulu-*.tar.gz \
  -C local/packages/pangulu --strip-components=1
tar -xzf local/downloads/linglongamg-*.tar.gz \
  -C local/packages/linglongamg --strip-components=1
tar -xzf local/downloads/xianzhikrylov-*.tar.gz \
  -C local/packages/xianzhikrylov --strip-components=1
```

The scripts load these package directories automatically.

### Step 6: Put the licenses in `local/licenses`

Copy one valid license for each required product into `local/licenses`.
Keep the downloaded file names.

```bash
ls -l local/licenses
```

### Step 7: Set the matrix and RHS paths

Set `DATA_ROOT` to the existing directory containing
`solverchallenge26_01` through `solverchallenge26_10`. Do not copy the data
files into this repository.

```bash
DATA_ROOT="/absolute/path/to/SolverChallenge26-preparation"
```

### Step 8: Build and run all problems

```bash
./scripts/run_all_problems.sh "$DATA_ROOT"
```

The script builds and runs all ten problems and prints the result directory.

### Find and read the results

```bash
latest_run="$(ls -dt results/run_* | head -n 1)"
printf 'Latest results: %s\n' "$latest_run"
cat "$latest_run/summary.tsv"
```

The files are stored as:

```text
results/run_<timestamp>/
├── summary.tsv
├── build-logs/
└── run-logs/
```

`PASS` means the solve and residual check succeeded. For a failed case, open
its log:

```bash
cat "$latest_run/run-logs/T01_q1.log"
```

### Step 9: Run one problem

Each problem can also be built and run independently.

#### Problems with one RHS: T03, T04, T05, T06, and T07

Replace `07` below with the required problem number:

```bash
./scripts/build_problem.sh 07
./scripts/run_problem.sh 07 \
  "$DATA_ROOT/solverchallenge26_07/solverchallenge26_07_A.dat" \
  "$DATA_ROOT/solverchallenge26_07/solverchallenge26_07_b.dat"
```

#### T01 and T02

Build the problem, then select q1 or q2:

```bash
./scripts/build_problem.sh 02
SC26_CASE=q1 ./scripts/run_problem.sh 02 \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_A.dat" \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_b.dat"
SC26_CASE=q2 ./scripts/run_problem.sh 02 \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_A.dat" \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_b.dat"
```

Use the same commands with `01` in place of `02` for T01. If `SC26_CASE` is
omitted, q1 is used.

#### T08, T09, or T10

These problems use one matrix and ten RHS files. The example below runs T10:

```bash
./scripts/build_problem.sh 10
CASE_ROOT="$DATA_ROOT/solverchallenge26_10"
./scripts/run_problem.sh 10 \
  "$CASE_ROOT/solverchallenge26_10_A.dat" \
  "$CASE_ROOT"/solverchallenge26_10_b_{01..10}.dat
```

For T08 or T09, change every `10` in the example to `08` or `09`.

## Problem and method map

| Problem | Preconditioner | Solver | RHS |
|---|---|---|---:|
| T01 | LinglongAMG | XianzhiKrylov FGMRES-DD; BiCGStab up to 4 steps inside each preconditioner application | 1 |
| T02 q1 | LinglongAMG | XianzhiKrylov PCG with constant-mode projection | 1 |
| T02 q2 | LinglongAMG | XianzhiKrylov FGMRES-DD; FGMRES up to 4 steps inside each preconditioner application | 1 |
| T03 | PanguLU | XianzhiKrylov FGMRES-DD | 1 |
| T04 | LinglongAMG elasticity AMG | XianzhiKrylov PCG | 1 |
| T05 | LinglongAMG | XianzhiKrylov BiCGStab | 1 |
| T06 | PanguLU | XianzhiKrylov complex FGMRES | 1 |
| T07 | XianzhiKrylov 3×3 block Jacobi | XianzhiKrylov BiCGStab | 1 |
| T08 | filtered ILU(0) | XianzhiKrylov batched complex FGMRES | 10 |
| T09 | none | PanguLU direct LU | 10 |
| T10 | none | PanguLU complex direct LU | 10 |

## Timing

Each program performs five warm-up rounds and five measured rounds. Iterative
methods run:

```text
ANALYSIS -> NUMERIC_PREPARE -> SOLVE
```

Direct methods run:

```text
ANALYSIS -> NUMERIC_FACTORISATION -> SOLVE
```

The reported time is:

```text
(end time - start time) / 5
```

Input reading, buffer copies, result copies, printing, and residual
verification are excluded. The programs print `CHECK end to end time` and
`COMPETITION_TIME_SECONDS`.

## Product API locations

- Problem configuration: `problems/problemNN/solver.c`
- PanguLU adapter: `src/product/pangulu_direct_solver.cpp`
- XianzhiKrylov adapter: `src/product/xianzhi_solver.c`
- Shared callback bridges: `problems/*_problem_helpers.h`

## Troubleshooting

- `Permission denied`: run `chmod +x scripts/*.sh`.
- `cmake` or `c++` is missing: load the corresponding cluster module.
- Package archive is missing: check `local/downloads`.
- Package file is missing: check `local/packages`.
- Custom package paths are needed: set them in `configs/local.env`.
- License error: keep one valid GPU-matching key in `local/licenses`.
- Shared library error: load the vendor runtime or set
  `SC26_HIP_RUNTIME_LIBRARY` in `configs/local.env`.
- Matrix or RHS is missing: check `DATA_ROOT`.
- GPU is missing: run the command inside an interactive GPU allocation.
- A case fails: open its log with the commands below.

```bash
latest_run="$(ls -dt results/run_* | head -n 1)"
cat "$latest_run/run-logs/T01_q1.log"
```

Replace `T01_q1.log` with the failed case shown in `summary.tsv`.
