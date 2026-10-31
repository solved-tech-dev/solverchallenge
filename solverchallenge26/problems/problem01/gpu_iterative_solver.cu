/* Problem 01 MPI GPU iterative adapter template. Replace only this adapter. */
#include "solverchallenge26/gpu_iterative_solver_api.h"

#include "solverchallenge26/gpu_metrics.h"

#include <stdlib.h>

struct SC26_GpuIterativeSolver {
    const struct SC26_SolverInput *input;
    struct GpuMemoryStats memory;
};

extern "C" int sc26_gpu_iterative_solver_create(
    const struct SC26_SolverInput *input,
    struct SC26_GpuIterativeSolver **solver)
{
    if (!input || !solver) return SC26_GPU_ERR_ARGUMENT;
    *solver = (struct SC26_GpuIterativeSolver *)calloc(1, sizeof(**solver));
    if (!*solver) return SC26_GPU_ERR_MEMORY;
    (*solver)->input = input;
    gpu_memory_begin(&(*solver)->memory);
    return SC26_GPU_SUCCESS;
}

extern "C" int sc26_gpu_iterative_solver_solve(
    struct SC26_GpuIterativeSolver *solver)
{
    (void)solver;
    /*
     * Add the selected external distributed GPU iterative solver here.
     * Use solver->input->comm and the local CSR view; do not gather
     * matrix data to rank 0. Keep iteration/convergence logic here.
     */
    return SC26_GPU_ERR_NOT_IMPLEMENTED;
}

extern "C" int sc26_gpu_iterative_solver_copy_solution(
    struct SC26_GpuIterativeSolver *solver)
{
    (void)solver;
    /*
     * Copy the local device solution into input->x/input->x_im so the
     * common harness can gather it with MPI_Gatherv.
     */
    return SC26_GPU_ERR_NOT_IMPLEMENTED;
}

extern "C" void sc26_gpu_iterative_solver_destroy(
    struct SC26_GpuIterativeSolver *solver)
{
    free(solver);
}

extern "C" int sc26_gpu_iterative_solver_get_memory_metrics(
    const struct SC26_GpuIterativeSolver *solver,
    struct SC26_GpuMemoryMetrics *metrics)
{
    if (!solver || !metrics) return SC26_GPU_ERR_ARGUMENT;
    metrics->tracked_peak_bytes = solver->memory.tracked_peak;
    metrics->observed_peak_bytes =
        solver->memory.baseline_free > solver->memory.minimum_free
            ? solver->memory.baseline_free - solver->memory.minimum_free
            : 0;
    return SC26_GPU_SUCCESS;
}
extern "C" int sc26_gpu_iterative_solver_upload(
    struct SC26_GpuIterativeSolver *solver)
{
    (void)solver;
    /* Upload the local CSR values and RHS here. This call is timed
     * separately and is excluded from the end-to-end solve time. */
    return SC26_GPU_ERR_NOT_IMPLEMENTED;
}
