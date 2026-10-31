#ifndef SC26_PUBLIC_GPU_ITERATIVE_SOLVER_API_H
#define SC26_PUBLIC_GPU_ITERATIVE_SOLVER_API_H

#include "gpu_direct_solver_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque per-run state owned by the selected problem's iterative GPU adapter. */
struct SC26_GpuIterativeSolver;

/*
 * The selected problem implements these functions. The adapter owns device
 * allocations and may use input->comm for distributed iterative operations.
 * The harness owns the host arrays referenced by SC26_SolverInput.
 */
int sc26_gpu_iterative_solver_create(
    const struct SC26_SolverInput *input,
    struct SC26_GpuIterativeSolver **solver);
/* Upload local host matrix/RHS and complete the transfer before returning. */
int sc26_gpu_iterative_solver_upload(
    struct SC26_GpuIterativeSolver *solver);
int sc26_gpu_iterative_solver_solve(
    struct SC26_GpuIterativeSolver *solver);
/* Copy the local device solution and complete the transfer before returning. */
int sc26_gpu_iterative_solver_copy_solution(
    struct SC26_GpuIterativeSolver *solver);
int sc26_gpu_iterative_solver_get_memory_metrics(
    const struct SC26_GpuIterativeSolver *solver,
    struct SC26_GpuMemoryMetrics *metrics);
void sc26_gpu_iterative_solver_destroy(
    struct SC26_GpuIterativeSolver *solver);

#ifdef __cplusplus
}
#endif

#endif
