# SolverChallenge26 Problem Solvers

SolverChallenge26 is the sixth edition of the annual linear solver algorithm and performance optimization competition organized by the SOLVER conference committee. It features ten linear systems drawn from real-world scientific and engineering applications, challenging participants to develop and optimize solvers under problem-specific accuracy and resource constraints on designated computing platforms.

This repository provides solver programs for all ten SolverChallenge26 problems using **PanguLU, LinglongAMG, and XianzhiKrylov**. The repository does not contain product libraries, licenses, matrices, or right-hand sides. You can download the libraries and licenses from [https://store.solved-tech.com/](https://store.solved-tech.com/).

**Competition and conference information**

- **SolverChallenge26 website:** [https://www.solver-conference.cn/solverchallenge26/](https://www.solver-conference.cn/solverchallenge26/)
- **SOLVER26 website:** [https://www.solver-conference.cn/](https://www.solver-conference.cn/)
- **Conference dates:** November 27–30, 2026
- **Location:** Shenzhen, Guangdong, China
- **Competition presentations and awards:** Held during SOLVER26; please refer to the conference website for the detailed schedule.

## What you need

You need:

- a Linux computer or an interactive GPU allocation from your cluster;
- a C and C++ compiler;
- CMake 3.21 or newer;
- Python 3;
- the vendor runtime required by the selected product package;
- the product packages and licenses for the problems you want to run;
- a site-provided glibc compatibility runtime when the host glibc is older
  than the selected package;
- the competition matrix and right-hand-side files.

Build and run inside a GPU allocation. Do not compile or run the solver on the
login node.

### Tested BW1000 cluster

The tested competition cluster runs Sugon OS 8.9 with glibc 2.28. Compute
nodes provide DTK 25.04.4 and 26.04 modules. Load the version named by the
downloaded package; the published 26.46 LinglongAMG BW1000 package requires
DTK 26.04. A typical one-GPU allocation is:

```bash
salloc \
  --partition=hx1hdnormal \
  --nodes=1 \
  --ntasks=1 \
  --cpus-per-task=16 \
  --gres=dcu:1 \
  --mem=58G \
  --time=02:00:00
srun --jobid="${SLURM_JOB_ID}" -N1 -n1 --pty bash
```

Adjust the partition and resource values if your cluster uses different
names. Run the remaining commands in the shell opened by `srun`:

```bash
source /etc/profile.d/modules.sh
module load compiler/dtk/26.04
command -v hipcc

export WORK_ROOT="/work2/${USER}"
export TMPDIR="${WORK_ROOT}/tmp/solverchallenge26"
mkdir -p "${WORK_ROOT}" "${TMPDIR}"

git clone https://github.com/solved-tech-dev/solverchallenge.git \
  "${WORK_ROOT}/solverchallenge"
cd "${WORK_ROOT}/solverchallenge/solverchallenge26"
```

Use cluster work storage rather than the login-node home directory or
`/tmp`. The tested cluster has a small `/tmp`; exhausting it can make CMake
report the misleading error `C compiler is not able to compile a simple test
program`.

The required products are:

| Problems | Packages and licenses |
|---|---|
| T01, T02, T04, T05 | LinglongAMG and XianzhiKrylov |
| T03, T06 | PanguLU and XianzhiKrylov |
| T07, T08 | XianzhiKrylov |
| T09, T10 | PanguLU |

## Repository layout

```text
.
├── apps/                       common test-program entry points
├── include/                    official SolverChallenge26 interfaces
├── src/                        input readers, residual checks, product adapters
├── problems/problem01..10/     solver settings and short solver notes
├── tests/                      template tests
├── configs/local.env.example   optional custom-path template
├── scripts/                    build, run, check, and summary commands
├── local/                      local packages and licenses; not committed
├── build/                      build output; created automatically
└── results/                    run output; created automatically
```

The `apps/`, `include/`, `src/`, `problems/`, and `tests/` directories follow
the public SolverChallenge26 template.

## Setup and Run

### Step 1: Get the project

If you followed the tested BW1000 cluster block above, the repository is
already under `/work2/${USER}`; skip the clone command in this step.

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
continuing. On compute nodes of the tested BW1000 cluster, the runtime is
loaded with:

```bash
source /etc/profile.d/modules.sh
module load compiler/dtk/26.04
```

Check the system C library version with `ldd --version` and compare it with
the package README. The tested XianzhiKrylov and LinglongAMG builds require
glibc2.35, while the tested PanguLU package requires glibc2.29. The tested
cluster only provides glibc2.28. On an older system, use a compatible package
or a compatibility runtime supplied by your administrator. For the latter,
place the complete runtime in `local/compat-runtime` and add this to
`configs/local.env`:

```bash
export SC26_COMPAT_ROOT="/absolute/path/to/solverchallenge26/local/compat-runtime"
```

The directory must contain `ld-linux-x86-64.so.2` and its matching libraries.
The script applies it only to the solver. Do not add that directory to the
shell's `LD_LIBRARY_PATH`.

The tested LinglongAMG package also requires `libmsgpackc.so.2`. If that
library is not installed system-wide, it must be present in the compatibility
runtime. Check it with:

```bash
test -e "${SC26_COMPAT_ROOT}/libmsgpackc.so.2"
find "${SC26_COMPAT_ROOT}" -maxdepth 1 \
  \( -name 'libgalaxyhip.so*' -o -name 'libamdhip64.so*' \) -print
```

When running on the tested cluster, also select the runtime explicitly in
`configs/local.env`:

```bash
export SC26_HIP_RUNTIME_LIBRARY=/public/software/compiler/dtk-26.04/hip/lib/libgalaxyhip.so.5
```

### Step 3: Create the local folders

Packages and licenses are deliberately kept outside Git. Run:

```bash
mkdir -p local/downloads \
         local/packages/pangulu \
         local/packages/linglongamg \
         local/packages/xianzhikrylov \
         local/licenses \
         local/compat-runtime
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
Keep the downloaded file names. Also place a copy in each package root:

```bash
cp local/licenses/pangulu-*.key local/packages/pangulu/license.key
cp local/licenses/linglongamg-*.key local/packages/linglongamg/license.key
cp local/licenses/xianzhikrylov-*.key local/packages/xianzhikrylov/license.key
```

Keep only the current key for each product in `local/licenses` before running
these commands. Each key must match the allocated GPU and remain valid for
the whole run.

The files in `local/licenses` are used by `check_environment.sh`. The product
libraries read the copies named `license.key` inside their package roots.
After renewing a license, replace both copies and remove stale keys.

```bash
ls -l local/licenses
```

### Step 7: Check the environment

Run:

```bash
./scripts/check_environment.sh
```

After setting `DATA_ROOT`, also check the matrix and RHS files:

```bash
./scripts/check_environment.sh "$DATA_ROOT"
```

The command prints clear `OK`, `WARN`, and `FAIL` messages.
The same `SC26_PROBLEMS` value used for the run limits the packages and data
files checked here.

### Step 8: Set the matrix and RHS paths

On the competition cluster, use the shared directory:

```bash
DATA_ROOT="/public/share/ac3k88uz8y/SolverChallenge26-TenProblems"
```

This directory contains `solverchallenge26_01` through
`solverchallenge26_10`. Do not copy the data files into this repository.
On another system, set `DATA_ROOT` or `SC26_DATA_ROOT` to your own directory.

### Step 9: Build and run problems

```bash
./scripts/run_all_problems.sh "$DATA_ROOT"
```

The script builds and runs all ten problems. To run only selected problems:

```bash
SC26_PROBLEMS=01,04,07 ./scripts/run_all_problems.sh "$DATA_ROOT"
```

The script prints the result directory and the final performance table.

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
├── performance.tsv
├── performance.json
├── build-logs/
├── run-logs/
└── solutions/
```

The script writes `performance.tsv` and `performance.json` automatically.
The TSV contains `case`, `status`, `time_ms`, `iterations`,
`max_relative_l2`, and `max_absolute_max`.
Each directory under `solutions/` contains the same `run.log`,
`performance.tsv`, `performance.json`, and `answer_x.rhs` files as a
single-problem run.
`summary.tsv` records build and run status for the pipeline; use
`performance.tsv` or `performance.json` for timing results.

`PASS` in a run row means the solve and residual check succeeded. For a failed case, open
its log:

```bash
cat "$latest_run/run-logs/T01_q1.log"
```

### Step 10: Run one problem

Each problem can also be built and run independently. The command prints
the result directory and a one-row performance table. A one-constraint
problem uses `results/TNN_<timestamp>/`; T01 and T02 use
`results/TNN_q1_<timestamp>/` and `results/TNN_q2_<timestamp>/`.

Every result directory contains:

```text
run.log
performance.tsv
performance.json
answer_x.rhs
```

`run.log` contains the complete solver output. `performance.tsv` and
`performance.json` use the same columns and fields as the all-problems run.
`answer_x.rhs`, when produced, contains the computed solution.

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

## Reference Environment

The programs target one BW1000 DCU. Select the DTK version named by the
product package. The published 26.46 LinglongAMG package validated with this
checkout uses DTK26.04.
The scripts select 4, 8, or 16 CPU threads according to the problem.
Per-problem runtime settings are in `problems/problemNN/config/runtime*.env`.
Each problem directory also has a short README with its solver and
preconditioner.

## Problem and method map

| Problem | Preconditioner | Solver | RHS |
|---|---|---|---:|
| [T01](problems/problem01/README.md) | LinglongAMG | XianzhiKrylov FGMRES | 1 |
| [T02 q1](problems/problem02/README.md) | LinglongAMG | XianzhiKrylov PCG | 1 |
| [T02 q2](problems/problem02/README.md) | LinglongAMG | XianzhiKrylov FGMRES | 1 |
| [T03](problems/problem03/README.md) | PanguLU | XianzhiKrylov FGMRES | 1 |
| [T04](problems/problem04/README.md) | LinglongAMG | XianzhiKrylov PCG | 1 |
| [T05](problems/problem05/README.md) | LinglongAMG | XianzhiKrylov FGMRES | 1 |
| [T06](problems/problem06/README.md) | PanguLU | XianzhiKrylov complex FGMRES | 1 |
| [T07](problems/problem07/README.md) | XianzhiKrylov block Jacobi | XianzhiKrylov BiCGStab | 1 |
| [T08](problems/problem08/README.md) | XianzhiKrylov ILU(0) | XianzhiKrylov complex FGMRES | 10 |
| [T09](problems/problem09/README.md) | none | PanguLU direct LU | 10 |
| [T10](problems/problem10/README.md) | none | PanguLU complex direct LU | 10 |

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

The reported `time_ms` is the mean of rounds 6 through 10. Each round has its
own timer, so printing statistics between rounds is not included.
`TRIAL trial=0..9` records these rounds (the first measured round is trial 5).

Input reading, matrix sorting, handle creation, result copying, and
independent residual verification happen outside these timers.
Restoring the zero initial guess is included in each solve. Repeated calls
with the same matrix descriptor reuse the completed analysis and numerical
preparation; the current RHS is rebound for each solve. Create a new matrix
descriptor if the matrix values or layout change. For ten RHS, one round
reuses its factorization across those RHS. Use the `time_ms` field in
`performance.tsv` as the reported time.

Timers are in `apps/iterative_solver.c`, `apps/iterative_solver_multi.c`, and
`apps/direct_solver_multi.c`. Library statistics follow each `TRIAL` line.
The final independent check uses the original A and b; single-RHS iterative
runs use binary128 arithmetic and return failure if their required residual
is not met. T08 checks complex absolute maximum residual for every RHS;
T09 and T10 check relative L2 residual for every RHS.

## Product API locations

- Problem configuration: `problems/problemNN/solver.c`
- PanguLU adapter: `src/product/pangulu_direct_solver.cpp`
- XianzhiKrylov adapter: `src/product/xianzhi_solver.c`
- Shared callback bridges: `problems/*_problem_helpers.h`

## Troubleshooting

- `Permission denied`: run `chmod +x scripts/*.sh`.
- `module: command not found`: run `source /etc/profile.d/modules.sh`.
- `Unable to locate a modulefile`: load the DTK module named in the selected
  product package on a compute node inside the Slurm allocation; the login
  node may not expose compiler modules.
- `C compiler is not able to compile a simple test program` with
  `No space left on device`: set `TMPDIR` below `/work2`, remove only the
  failed `build/problemNN` directory, and rerun the build.
- `GLIBC_2.29 not found` or `GLIBC_2.35 not found`: configure the complete
  compatibility runtime through `SC26_COMPAT_ROOT`.
- `libmsgpackc.so.2: cannot open shared object file`: add the library to the
  compatibility runtime or install the same version system-wide.
- `hip: driver library unavailable`: load DTK inside the GPU allocation and
  run the GPU-identity tool on the allocated node.
- Package archive is missing: check `local/downloads`.
- Package file is missing: check `local/packages`.
- Custom package paths are needed: set them in `configs/local.env`.
- `license.key` not found: place the key both in `local/licenses` and in the
  corresponding package root.
- License expired or machine mismatch: request a key for the current GPU
  allocation; do not reuse a key issued for another card.
- `BATCH_ERROR out of memory` in T08: use the current 26.46 XianzhiKrylov
  package. Do not reduce `batch`; that was not a numerically valid workaround.
- Shared library error: load the vendor runtime or set
  `SC26_HIP_RUNTIME_LIBRARY` in `configs/local.env`.
- `Permission denied` when reading the competition data directory: ask the
  cluster administrator for access to the competition data, or pass an
  existing readable mirror through `DATA_ROOT`. Do not copy the full data set
  into the repository.
- Matrix or RHS is missing: check `DATA_ROOT`.
- GPU is missing: run the command inside an interactive GPU allocation.
- A case fails: open its log with the commands below.

```bash
latest_run="$(ls -dt results/run_* | head -n 1)"
cat "$latest_run/run-logs/T01_q1.log"
```

Replace `T01_q1.log` with the failed case shown in `summary.tsv`.
