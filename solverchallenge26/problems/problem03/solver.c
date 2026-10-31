#include "solver.h"

#include "../xianzhi_problem_helpers.h"

#include <stdlib.h>

#ifndef SC26_SOLUTION_FP128
#error "T03 requires SC26_SOLUTION_FP128=ON"
#endif

void sc26_problem_create(void *solver, const struct SC26_SolverInput *input)
{
    char config[4096];
    sc26_problem_config_path(config, sizeof(config), "solver.json");
    sc26_problem_create_xianzhi(
        solver, input, config, 0, 1,
        getenv("SC26_PANGULU_LIBRARY"), NULL);
}

#ifdef ITERATIVE_SOLVER
SC26_DEFINE_XIANZHI_ITERATIVE_CALLBACKS()
#endif
#ifdef DIRECT_SOLVER
SC26_DEFINE_XIANZHI_DIRECT_CALLBACKS()
#endif
SC26_DEFINE_XIANZHI_DESTROY()
