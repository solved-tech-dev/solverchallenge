# Local product packages

Product packages and licenses are intentionally excluded from this repository.
After downloading the release matching the target GPU and toolkit, extract:

```text
local/
├── packages/
│   ├── pangulu/
│   │   ├── include/
│   │   └── lib/
│   ├── linglongamg/
│   │   └── lib/
│   └── xianzhikrylov/
│       ├── include/
│       └── lib/
└── licenses/
    ├── pangulu.license.key
    ├── linglongamg.license.key
    └── xianzhikrylov.license.key
```

Each package directory must contain the package payload directly. If an
archive creates an additional top-level version directory, either remove that
extra level or point to the real path from `configs/local.env`.

Typical extraction commands:

```bash
mkdir -p local/packages/pangulu \
         local/packages/linglongamg \
         local/packages/xianzhikrylov \
         local/licenses

tar -xf PANGULU_PACKAGE.tar.gz \
  -C local/packages/pangulu --strip-components=1
tar -xf LINGLONGAMG_PACKAGE.tar.gz \
  -C local/packages/linglongamg --strip-components=1
tar -xf XIANZHIKRYLOV_PACKAGE.tar.gz \
  -C local/packages/xianzhikrylov --strip-components=1
```

Use the actual archive names and the package matching the installed DTK
environment. Always verify that `include/` and `lib/` are directly visible
below each package directory.

The helper scripts load these paths automatically:

```bash
./scripts/run_all_problems.sh /path/to/SolverChallenge26-preparation

./scripts/build_problem.sh 07
./scripts/run_problem.sh 07 MATRIX RHS
```

`run_all_problems.sh` writes build and run logs plus `summary.tsv` under
`results/run_<timestamp>/`.

No file below `local/packages/` or `local/licenses/` is committed to Git.
