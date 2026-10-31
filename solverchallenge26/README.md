# SolverChallenge26 programs

This repository contains ready-to-build test programs for all ten
SolverChallenge26 problems.

The repository does not contain product libraries, licenses, matrices, or
right-hand sides. You can download the libraries and licenses from
[https://store.solved-tech.com/](https://store.solved-tech.com/).

Run the commands on the target GPU machine in one terminal window. Type or
paste one command block, wait for it to finish, and then run the next block.
The command blocks do not include a shell prompt. Do not type `$`.

## What you need

You need:

- a Linux computer or an interactive GPU allocation from your cluster;
- a C and C++ compiler;
- CMake 3.21 or newer;
- the vendor runtime required by the selected product package;
- the product packages and licenses for the problems you want to run;
- the competition matrix and right-hand-side files.

Do not use a normal login node for a full run. Start an interactive GPU
allocation by using the method provided by your cluster.

The required products are:

| Problems | Packages and licenses |
|---|---|
| T01, T02, T05 | LinglongAMG and XianzhiKrylov |
| T03, T06 | PanguLU and XianzhiKrylov |
| T07, T08 | XianzhiKrylov |
| T09, T10 | PanguLU |

For a first test, use T07. It needs only XianzhiKrylov.

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
├── local/                      local packages, licenses, and data; not committed
├── build/                      build output; created automatically
└── results/                    run output; created automatically
```

The `apps/`, `include/`, `src/`, `problems/`, and `tests/` directories follow
the public SolverChallenge26 template.

## Step 1: Open a terminal and get the project

If a terminal is not already open, press `Ctrl+Alt+T` on many Linux desktops,
or open the project folder in the file manager, right-click an empty area, and
choose an item such as **Open in Terminal**. Use `Ctrl+Shift+V` to paste a
command; `Ctrl+V` may not work in a terminal.

If Git is available, run:

```bash
git clone https://github.com/solved-tech-dev/solverchallenge.git
cd solverchallenge/solverchallenge26
```

The first line downloads the repository. The second line enters this
SolverChallenge26 project inside it.

All remaining commands must be run from this `solverchallenge26` folder.
Linux commands distinguish uppercase and lowercase letters, so type them as
shown. `sudo` is not required.

If the project was downloaded as an archive, extract it with the file manager,
open a terminal in the extracted `solverchallenge-main/solverchallenge26`
folder, and run:

```bash
pwd
ls
```

`pwd` must end in `solverchallenge26`, and `ls` must show `scripts`. If you do
not see `scripts`, you are in the wrong folder.

Make the helper scripts executable:

```bash
chmod +x scripts/*.sh
```

## Step 2: Check the basic tools

Run:

```bash
cmake --version
c++ --version
```

Each command must print a version. If you see `command not found`, ask your
cluster administrator how to load CMake and the C++ compiler. On a cluster,
this is usually done with a `module load` command specific to that cluster.

If the Hygon runtime is loaded with modules at your site, load it before
building and running the programs.

## Step 3: Create the local folders

Packages, licenses, and data are deliberately kept outside Git. Run:

```bash
mkdir -p local/downloads \
         local/packages/pangulu \
         local/packages/linglongamg \
         local/packages/xianzhikrylov \
         local/licenses \
         local/data
```

Nothing below `local/` is uploaded to Git.

## Step 4: Put the downloaded packages in `local/downloads`

Download the product packages matching the target GPU and installed vendor
runtime. Do not mix packages built for different GPU runtimes.

Using the file manager is the easiest method:

1. Open `local/downloads` in the file manager.
2. Copy the downloaded package archives into that folder.
3. Keep exactly one PanguLU archive, one LinglongAMG archive, and one
   XianzhiKrylov archive.

If the archives are in the default Downloads folder, they can also be copied
from a terminal. Here `~` means your home folder, usually
`/home/YOUR_USER_NAME`. Replace `FILE_NAME` with the real names shown by
`ls ~/Downloads`:

```bash
ls ~/Downloads
cp ~/Downloads/FILE_NAME_PANGULU.tar.gz local/downloads/
cp ~/Downloads/FILE_NAME_LINGLONGAMG.tar.gz local/downloads/
cp ~/Downloads/FILE_NAME_XIANZHIKRYLOV.tar.gz local/downloads/
```

You may skip the PanguLU and LinglongAMG commands when you only want to run
T07 or T08.

Check what is present:

```bash
find local/downloads -maxdepth 1 -type f -name '*.tar.gz' -print
```

The output must list the archives you intend to install.

## Step 5: Extract the packages

Extract each product you need. The commands below find the archive
automatically, so the version and date in its name do not need to be typed.

### PanguLU, when needed

```bash
PANGULU_ARCHIVE="$(find local/downloads -maxdepth 1 -type f \
  -name 'pangulu-*.tar.gz' -print -quit)"
printf 'PanguLU archive: %s\n' "$PANGULU_ARCHIVE"
tar -xzf "$PANGULU_ARCHIVE" \
  -C local/packages/pangulu --strip-components=1
```

### LinglongAMG, when needed

```bash
AMG_ARCHIVE="$(find local/downloads -maxdepth 1 -type f \
  -name 'linglongamg-*.tar.gz' -print -quit)"
printf 'LinglongAMG archive: %s\n' "$AMG_ARCHIVE"
tar -xzf "$AMG_ARCHIVE" \
  -C local/packages/linglongamg --strip-components=1
```

### XianzhiKrylov

```bash
XK_ARCHIVE="$(find local/downloads -maxdepth 1 -type f \
  -name 'xianzhikrylov-*.tar.gz' -print -quit)"
printf 'XianzhiKrylov archive: %s\n' "$XK_ARCHIVE"
tar -xzf "$XK_ARCHIVE" \
  -C local/packages/xianzhikrylov --strip-components=1
```

The helper scripts expect the package contents directly below each product
folder. The important files must be at these locations:

```text
local/packages/pangulu/include/pangulu.h
local/packages/pangulu/lib/libpangulu.so
local/packages/linglongamg/lib/liblinglongamg.so
local/packages/xianzhikrylov/include/XianzhiKrylovAPI.h
local/packages/xianzhikrylov/lib/libXianzhiKrylov.so
```

Check the files for the products you installed:

```bash
test -f local/packages/pangulu/include/pangulu.h && echo "PanguLU header: OK"
test -f local/packages/pangulu/lib/libpangulu.so && echo "PanguLU library: OK"
test -f local/packages/linglongamg/lib/liblinglongamg.so && echo "LinglongAMG library: OK"
test -f local/packages/xianzhikrylov/include/XianzhiKrylovAPI.h && echo "XianzhiKrylov header: OK"
test -f local/packages/xianzhikrylov/lib/libXianzhiKrylov.so && echo "XianzhiKrylov library: OK"
```

The checks for products you do not need may print nothing.

If an archive contains an extra top-level folder, the `--strip-components=1`
command normally removes it. If a required file is still in a different
location, use the custom-path method in the Troubleshooting section.

## Step 6: Put the licenses in `local/licenses`

Using the file manager is again the easiest method:

1. Open `local/licenses`.
2. Copy the downloaded license files into this folder.
3. Keep only one valid license for each product.

The file names only need to match these patterns:

```text
local/licenses/pangulu*.key
local/licenses/linglongamg*.key
local/licenses/xianzhikrylov*.key
```

For example:

```bash
ls -l local/licenses
```

Each license must match the actual GPU on which you run the program. An
expired license or a license issued for another machine will not work.

## Step 7: Put the matrix and RHS data in place

The competition data folder must contain:

```text
SolverChallenge26-preparation/
├── solverchallenge26_01/
│   ├── solverchallenge26_01_A.dat
│   └── solverchallenge26_01_b.dat
├── solverchallenge26_02/
│   ├── solverchallenge26_02_A.dat
│   └── solverchallenge26_02_b.dat
├── ...
└── solverchallenge26_10/
    ├── solverchallenge26_10_A.dat
    ├── solverchallenge26_10_b_01.dat
    ├── solverchallenge26_10_b_02.dat
    ├── ...
    └── solverchallenge26_10_b_10.dat
```

The data may be copied into:

```text
local/data/SolverChallenge26-preparation
```

When the data is already on a shared disk or NAS, it does not need to be
copied. Ask the person who supplied it for the full path.

For the commands below, store that path in a variable. An absolute path starts
with `/`. Replace everything between the double quotes with the real path:

```bash
DATA_ROOT="/absolute/path/to/SolverChallenge26-preparation"
test -d "$DATA_ROOT" && echo "Data folder: OK"
```

To check T07:

```bash
ls -l "$DATA_ROOT/solverchallenge26_07"
```

You must see both `solverchallenge26_07_A.dat` and
`solverchallenge26_07_b.dat`.

## Recommended first run: T07

Run this after installing only XianzhiKrylov:

```bash
./scripts/build_problem.sh 07
```

The last line must contain:

```text
build complete:
```

Now set the two input paths:

```bash
MATRIX="$DATA_ROOT/solverchallenge26_07/solverchallenge26_07_A.dat"
RHS="$DATA_ROOT/solverchallenge26_07/solverchallenge26_07_b.dat"
ls -l "$MATRIX" "$RHS"
```

Run T07:

```bash
./scripts/run_problem.sh 07 "$MATRIX" "$RHS"
```

A successful run contains:

```text
COMPETITION_TIME_SECONDS=
```

and residual lines ending in `PASS`.

Do not continue to the full run until this first test succeeds.

## Run all problems

After all required packages, licenses, and data are installed, run:

```bash
./scripts/run_all_problems.sh "$DATA_ROOT"
```

This command builds and runs T01 through T10. It runs both constraints for T01
and T02 and uses all ten RHS files for T08, T09, and T10.

The run may take a long time. Do not close the terminal.

When it finishes, the terminal prints the exact output folder, for example:

```text
Results: /home/alice/solverchallenge26/results/run_20261031T120000+0800
Summary: /home/alice/solverchallenge26/results/run_20261031T120000+0800/summary.tsv
```

Your date and path will be different. Do not type `<timestamp>`.

## Find and read the results

The following commands find the newest run, so you do not need to type its
timestamp:

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

`summary.tsv` has three columns: case name, status, and a log path or input
folder. The status values mean:

- `PASS`: the program completed and the residual check passed.
- `FAIL`: the program failed. Read the log path in the third column.
- `INPUT_MISSING`: a matrix or RHS file is missing or unreadable.
- `SKIPPED`: the case is intentionally not run.

To read a failed T01 q1 log:

```bash
cat "$latest_run/run-logs/T01_q1.log"
```

Other log names include:

```text
T01_q2.log
T02_q1.log
T02_q2.log
T03.log
T05.log
T06.log
T07.log
T08.log
T09.log
T10.log
```

Read from the first error, not only from the final line.

To use a different output folder:

```bash
./scripts/run_all_problems.sh "$DATA_ROOT" my-results
```

## Run only one other problem

### T01 and T02

Build the problem first:

```bash
./scripts/build_problem.sh 01
```

Run q1 or q2:

```bash
SC26_CASE=q1 ./scripts/run_problem.sh 01 \
  "$DATA_ROOT/solverchallenge26_01/solverchallenge26_01_A.dat" \
  "$DATA_ROOT/solverchallenge26_01/solverchallenge26_01_b.dat"

SC26_CASE=q2 ./scripts/run_problem.sh 01 \
  "$DATA_ROOT/solverchallenge26_01/solverchallenge26_01_A.dat" \
  "$DATA_ROOT/solverchallenge26_01/solverchallenge26_01_b.dat"
```

For T02, change `01` to `02`:

```bash
./scripts/build_problem.sh 02
SC26_CASE=q1 ./scripts/run_problem.sh 02 \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_A.dat" \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_b.dat"
SC26_CASE=q2 ./scripts/run_problem.sh 02 \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_A.dat" \
  "$DATA_ROOT/solverchallenge26_02/solverchallenge26_02_b.dat"
```

If `SC26_CASE` is omitted, q1 is used.

### T03, T05, T06, or T07

These problems use one RHS. For example, for T05:

```bash
./scripts/build_problem.sh 05
./scripts/run_problem.sh 05 \
  "$DATA_ROOT/solverchallenge26_05/solverchallenge26_05_A.dat" \
  "$DATA_ROOT/solverchallenge26_05/solverchallenge26_05_b.dat"
```

For another problem, change every `05` in the block to the problem number.

### T08, T09, or T10

These problems use one matrix and ten RHS files. For example, for T10:

```bash
./scripts/build_problem.sh 10
CASE_ROOT="$DATA_ROOT/solverchallenge26_10"
./scripts/run_problem.sh 10 \
  "$CASE_ROOT/solverchallenge26_10_A.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_01.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_02.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_03.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_04.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_05.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_06.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_07.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_08.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_09.dat" \
  "$CASE_ROOT/solverchallenge26_10_b_10.dat"
```

For T08 or T09, change every `10` in the block to `08` or `09`.

## Problem and method map

| Problem | Preconditioner | Method | RHS |
|---|---|---|---:|
| T01 | LinglongAMG | XianzhiKrylov FGMRES-DD; BiCGStab up to 4 steps inside each preconditioner application | 1 |
| T02 q1 | LinglongAMG | XianzhiKrylov PCG with constant-mode projection | 1 |
| T02 q2 | LinglongAMG | XianzhiKrylov FGMRES-DD; FGMRES up to 4 steps inside each preconditioner application | 1 |
| T03 | PanguLU | XianzhiKrylov FGMRES-DD | 1 |
| T04 | LinglongAMG elasticity AMG | XianzhiKrylov PCG | 1 |
| T05 | LinglongAMG | XianzhiKrylov BiCGStab | 1 |
| T06 | PanguLU | XianzhiKrylov complex FGMRES | 1 |
| T07 | XianzhiKrylov 3×3 block Jacobi | XianzhiKrylov hybrid BiCGStab | 1 |
| T08 | filtered ILU(0) | XianzhiKrylov batched complex FGMRES | 10 |
| T09 | none | PanguLU direct LU | 10 |
| T10 | none | PanguLU complex direct LU | 10 |

`PCG with constant-mode projection` does not modify the final solution after
solving. It builds a mean-zero working residual for the PCG iteration and
uses the original RHS for the final residual check.

The short BiCGStab or FGMRES process in T01 and T02 is part of one outer
FGMRES-DD preconditioner application. It is not a second independent solve.

## How competition time is measured

Each program performs five warm-up rounds and then five measured rounds.

For an iterative method, each measured round runs:

```text
ANALYSIS -> NUMERIC_PREPARE -> SOLVE
```

For a direct method, each measured round runs:

```text
ANALYSIS -> NUMERIC_FACTORISATION -> SOLVE
```

Timing starts immediately before the first timed `ANALYSIS`. It ends after
the fifth timed `SOLVE` and GPU synchronization.

The competition time is:

```text
(end time - start time) / 5
```

The time does not include:

- reading matrix or RHS files;
- creating solver, matrix, and vector objects;
- copying the matrix from the CPU to the GPU;
- copying the solution back to the CPU;
- printing or residual verification.

The programs print the result in two forms:

- `CHECK end to end time`, in milliseconds;
- `COMPETITION_TIME_SECONDS`, in seconds.

A run passes only when the program exits normally and every required residual
line contains `PASS`.

## Where the product APIs are called

The common test programs do not call vendor APIs directly:

```text
apps/
  -> problems/problemNN/solver.c
  -> callback helper
  -> src/product/
  -> PanguLU or XianzhiKrylov
```

- PanguLU direct API calls: `src/product/pangulu_direct_solver.cpp`
- PanguLU callback bridge: `problems/pangulu_problem_helpers.h`
- XianzhiKrylov API calls: `src/product/xianzhi_solver.c`
- XianzhiKrylov callback bridge: `problems/xianzhi_problem_helpers.h`

Each `problems/problemNN/solver.c` selects the configuration and connects the
official solver callback to the product adapter.

## Troubleshooting

When a command fails, first confirm that you are in the project folder:

```bash
pwd
ls
```

The folder must contain `scripts`.

### `Permission denied`

Run:

```bash
chmod +x scripts/*.sh
```

### `cmake: command not found` or `c++: command not found`

Ask your cluster administrator for the module command that loads CMake and a
C++17 compiler. After loading the module, run `cmake --version` again.

### `tar: ...: Cannot open: No such file or directory`

Check that the archive is in `local/downloads`:

```bash
find local/downloads -maxdepth 1 -type f -name '*.tar.gz' -print
```

If the file is not listed, copy it into that folder.

### A package file cannot be found

Check the product folder:

```bash
find local/packages -maxdepth 4 -type f \
  \( -name 'pangulu.h' -o -name 'libpangulu.so' \
     -o -name 'liblinglongamg.so' \
     -o -name 'XianzhiKrylovAPI.h' -o -name 'libXianzhiKrylov.so' \) -print
```

If the files are under different absolute paths, create a custom path file:

```bash
cp configs/local.env.example configs/local.env
```

Open `configs/local.env` in a text editor and remove the `#` from the required
lines. Put the real absolute paths after `=`.

Example:

```text
PANGULU_ROOT=/home/alice/products/pangulu
SC26_LINGLONGAMG_LIBRARY=/home/alice/products/linglongamg/lib/liblinglongamg.so
XIANZHIKRYLOV_ROOT=/home/alice/products/xianzhikrylov
```

### `License failure`, `LICENSE_EXPIRED`, or an authorization error

Check:

- the license file is under `local/licenses`;
- only one valid license for the product is present;
- the license matches the GPU used by the command;
- the license has not expired.

List the installed licenses:

```bash
ls -l local/licenses
```

If the license has expired, request a new license for the same GPU.

### `error while loading shared libraries`

The matching product package or Hygon runtime is not available at run time.

First confirm that the product package was extracted into the expected folders.
If the runtime is loaded with modules at your site, load it before running the
program.

If the Hygon runtime is installed at a nonstandard path, set
`SC26_HIP_RUNTIME_LIBRARY` in `configs/local.env` to the full path of the
matching `libamdhip64.so` or `libgalaxyhip.so`.

### `Matrix file not found` or `RHS file not found`

Check that `DATA_ROOT` is an absolute path and that it points to the folder
containing `solverchallenge26_01` through `solverchallenge26_10`:

```bash
printf '%s\n' "$DATA_ROOT"
find "$DATA_ROOT" -maxdepth 2 -type f | head -n 20
```

For T08, T09, and T10, make sure all ten files from `_b_01.dat` through
`_b_10.dat` exist.

### The GPU is not found

You are probably running on a login node instead of an interactive GPU
allocation. Exit and start an interactive GPU session with the command
provided by your cluster.

### A problem ends with `FAIL`

Open its log:

```bash
latest_run="$(ls -dt results/run_* | head -n 1)"
cat "$latest_run/run-logs/T01_q1.log"
```

Replace `T01_q1.log` with the failed case shown in `summary.tsv`.

When asking for help, send:

1. the command you ran;
2. the first error line and several lines below it;
3. the related log file.

Do not send only “it does not work”.
