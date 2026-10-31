#include "solver.h"

#include "../pangulu_problem_helpers.h"

void sc26_problem_create(void *solver, const struct SC26_SolverInput *input)
{
    struct MySolver *state = (struct MySolver *)solver;
    sc26_pangulu_require(
        sc26_pangulu_direct_solver_create(
            (struct sc26_pangulu_direct_solver **)&state->product, input),
        "PanguLU solver creation");
}

#ifdef DIRECT_SOLVER
SC26_DEFINE_PANGULU_DIRECT_CALLBACKS()
#endif
#ifdef ITERATIVE_SOLVER
SC26_DEFINE_PANGULU_ITERATIVE_CALLBACKS()
#endif
SC26_DEFINE_PANGULU_DESTROY()
