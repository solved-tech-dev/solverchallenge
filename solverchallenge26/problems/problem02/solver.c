#include "solver.h"

#include "../xianzhi_problem_helpers.h"

#include <stdlib.h>
#include <string.h>

#ifndef SC26_SOLUTION_FP128
#error "T02 requires SC26_SOLUTION_FP128=ON"
#endif

void sc26_problem_create(void *solver, const struct SC26_SolverInput *input)
{
    const char *case_name = getenv("SC26_CASE");
    int q2;
    char config[4096];
    char amg_config[4096];
    if (case_name && *case_name && strcmp(case_name, "q1") != 0 &&
        strcmp(case_name, "q2") != 0) {
        fprintf(stderr, "SC26_CASE must be q1 or q2\n");
        exit(EXIT_FAILURE);
    }
    q2 = case_name && strcmp(case_name, "q2") == 0;
    sc26_problem_config_path(
        config, sizeof(config), q2 ? "solver_q2.json" : "solver_q1.json");
    sc26_problem_config_path(
        amg_config, sizeof(amg_config), q2 ? "amg_q2.json" : "amg_q1.json");
    sc26_problem_create_xianzhi(
        solver, input, config, 0, q2,
        getenv("SC26_LINGLONGAMG_LIBRARY"), amg_config);
}

#ifdef ITERATIVE_SOLVER
SC26_DEFINE_XIANZHI_ITERATIVE_CALLBACKS()
#endif
#ifdef DIRECT_SOLVER
SC26_DEFINE_XIANZHI_DIRECT_CALLBACKS()
#endif
SC26_DEFINE_XIANZHI_DESTROY()
