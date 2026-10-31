#include "solver.h"

#include "../xianzhi_problem_helpers.h"

#include <stdlib.h>

void sc26_problem_create(void *solver, const struct SC26_SolverInput *input)
{
    char config[4096];
    char amg_config[4096];
    sc26_problem_config_path(config, sizeof(config), "solver_q1.json");
    sc26_problem_config_path(amg_config, sizeof(amg_config), "amg_q1.json");
    sc26_problem_create_xianzhi(
        solver, input, config, 0, 0,
        getenv("SC26_LINGLONGAMG_LIBRARY"), amg_config);
}

#ifdef ITERATIVE_SOLVER
SC26_DEFINE_XIANZHI_ITERATIVE_CALLBACKS()
#endif
#ifdef DIRECT_SOLVER
SC26_DEFINE_XIANZHI_DIRECT_CALLBACKS()
#endif
SC26_DEFINE_XIANZHI_DESTROY()
