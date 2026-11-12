#include "solver.h"

#include "../pangulu_problem_helpers.h"

#include <stddef.h>

void sc26_problem_create(void *solver, const struct SC26_SolverInput *input)
{
    struct MySolver *state = (struct MySolver *)solver;
    sc26_pangulu_require(
        sc26_pangulu_direct_solver_create(
            (struct sc26_pangulu_direct_solver **)&state->product, input),
        "PanguLU solver creation");
}

#ifdef DIRECT_SOLVER
int sc26_problem_gpu_memory_metrics(
    void *solver,
    size_t *peak_bytes,
    size_t *total_bytes,
    int *available)
{
    struct MySolver *state = (struct MySolver *)solver;
    return sc26_pangulu_direct_solver_memory_metrics(
        state ? (struct sc26_pangulu_direct_solver *)state->product : NULL,
        peak_bytes, total_bytes, available);
}

SC26_DEFINE_PANGULU_DIRECT_CALLBACKS()
#endif
#ifdef ITERATIVE_SOLVER
SC26_DEFINE_PANGULU_ITERATIVE_CALLBACKS()
#endif
SC26_DEFINE_PANGULU_DESTROY()
