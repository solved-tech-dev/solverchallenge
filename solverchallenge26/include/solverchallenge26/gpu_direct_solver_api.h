#ifndef SC26_PUBLIC_GPU_DIRECT_SOLVER_API_H
#define SC26_PUBLIC_GPU_DIRECT_SOLVER_API_H

#include "solver_input.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque per-run state owned by the selected problem's GPU adapter. */
struct SC26_GpuDirectSolver;

enum {
    SC26_GPU_SUCCESS = 0,
    SC26_GPU_ERR_ARGUMENT = 1,
    SC26_GPU_ERR_MEMORY = 2,
    SC26_GPU_ERR_NOT_IMPLEMENTED = 3
};

struct SC26_GpuMemoryMetrics {
    size_t tracked_peak_bytes;
    size_t observed_peak_bytes;
};

/*
 * The selected problem implements these functions.  The adapter owns all
 * device allocations and may use input->comm for distributed communication.
 * The harness owns the host arrays referenced by SC26_SolverInput.
 */
int sc26_gpu_direct_solver_create(const struct SC26_SolverInput *input,
                                  struct SC26_GpuDirectSolver **solver);
/* Upload local host matrix/RHS and complete the transfer before returning. */
int sc26_gpu_direct_solver_upload(struct SC26_GpuDirectSolver *solver);
int sc26_gpu_direct_solver_solve(struct SC26_GpuDirectSolver *solver);
/* Copy the local device solution and complete the transfer before returning. */
int sc26_gpu_direct_solver_copy_solution(
    struct SC26_GpuDirectSolver *solver);
int sc26_gpu_direct_solver_get_memory_metrics(
    const struct SC26_GpuDirectSolver *solver,
    struct SC26_GpuMemoryMetrics *metrics);
void sc26_gpu_direct_solver_destroy(struct SC26_GpuDirectSolver *solver);

#ifdef __cplusplus
}
#endif

#endif
