# Problem slots

The competition template provides ten independent solver modification slots:
`problem01` through `problem10`. Put the interface declarations in that
directory's `solver.h` and the implementation in `solver.c`; the shared
readers, MPI distribution, timing, memory reporting, and residual checks are
reused from `src/`.

The populated slots in this repository are:

| Slot | Callback type | Preconditioner | Outer method | Inner short Krylov |
|---|---|---|---|---|
| problem01 | iterative | LinglongAMG | XianzhiKrylov FGMRES-DD | BiCGStab, max 4 steps |
| problem02 q1 | iterative | LinglongAMG | XianzhiKrylov Projected PCG | none |
| problem02 q2 | iterative | LinglongAMG | XianzhiKrylov FGMRES-DD | FGMRES, max 4 steps |
| problem03 | iterative | PanguLU | XianzhiKrylov FGMRES-DD | none |
| problem04 | not selected | none | placeholder | none |
| problem05 | iterative | LinglongAMG | XianzhiKrylov BiCGStab | none |
| problem06 | iterative | PanguLU | XianzhiKrylov complex FGMRES | none |
| problem07 | iterative | XianzhiKrylov 3×3 block Jacobi | XianzhiKrylov BiCGStab | none |
| problem08 | iterative | filtered ILU(0) | XianzhiKrylov batched complex FGMRES | none |
| problem09 | direct | none | PanguLU direct LU | none |
| problem10 | direct | none | PanguLU complex direct LU | none |

`inner_method` is a short Krylov process used inside each preconditioner
application of the outer `fgmres_dd`. It does not perform the final
convergence check. The suffix `4` means a maximum of four inner iterations.

Problems 09 and 10 use `src/product/pangulu_direct_solver.cpp`. The
XianzhiKrylov problems use the common adapter in
`src/product/xianzhi_solver.c`. These files contain product calls and device
buffer management only; they do not contain a program entry point or re-read
matrix files.

`SC26_SolverInput` keeps the original single-RHS fields and adds
`rhs_count`, `b_all`, `b_im_all`, `x_all`, and `x_im_all` for the ten-RHS
problems. The original serial and MPI applications initialize the new fields
as one-RHS aliases, so the official data contract remains valid.

Choose the slot at configure time with `-DSC26_PROBLEM=01` through
`-DSC26_PROBLEM=10`. The selected slot is added to the include path and its
`solver.c` is compiled with the existing solver entry points. Other problem
implementations are not compiled.

Each problem also has a `CMakeLists.txt` containing the
`sc26_problem_dependencies` interface target. Add `find_package`, include
directories, compile definitions, and link libraries for that problem there;
the common build automatically propagates them to its serial and MPI solver
targets.

When MPI GPU support is enabled, the selected problem's `gpu_direct_solver.cu`
and `gpu_iterative_solver.cu` are compiled as the direct and iterative GPU
adapters. Each adapter receives the local CSR view and `MPI_Comm` through
`SC26_SolverInput`. Implement host-to-device copies in the adapter's `upload`
function and device-to-host solution copies in `copy_solution`; both transfer
times are reported separately from the end-to-end solve time.
Use the shared `gpu_memory_sample` helper after allocation-heavy stages so the
reported tracked peak and observed memory delta retain the reference semantics.
The direct adapter is intended for factorization/direct-solve libraries,
whereas the iterative adapter owns iteration, convergence, and any required
distributed reductions.
Keep external GPU library discovery and linking in the same problem's
`CMakeLists.txt`.
