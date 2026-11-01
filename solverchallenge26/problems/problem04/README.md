# T04: PCG with elasticity AMG

Build and run from the project directory as shown in the main README.
`config/solver_q1.json` selects XianzhiKrylov PCG with LinglongAMG.
`config/runtime.env` supplies the tested BW1000 settings.

## Device-input preparation

With the LinglongAMG device-input candidate dated 2026-11-01, select:

```bash
SC26_T4_DEVICE_INPUT=1 ./scripts/run_problem.sh 04 /path/to/A.dat /path/to/b.dat
```

This uses `config/solver_device.json` and enables `SA6_DEVICE_INPUT=1`.
It removes the full CSR copy back to CPU. CPU structure work and selected
local data transfers remain. Do not enable it with an older LinglongAMG
package. The default configuration retains the previously tested path.

The candidate reduces elapsed wall time, but its measured computation time
after excluding transfers is currently higher than the default. Both
configurations use the same original-system residual threshold of 1e-8.
