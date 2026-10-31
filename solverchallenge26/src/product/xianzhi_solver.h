#ifndef SC26_PRODUCT_XIANZHI_SOLVER_H
#define SC26_PRODUCT_XIANZHI_SOLVER_H

#include "solverchallenge26/solver_input.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sc26_xianzhi_solver;

struct sc26_xianzhi_options {
    const char *config_template;
    const char *provider_library;
    const char *provider_config;
    int complex_values;
    int dd_solution;
};

int sc26_xianzhi_solver_create(
    struct sc26_xianzhi_solver **solver,
    const struct SC26_SolverInput *input,
    const struct sc26_xianzhi_options *options);

int sc26_xianzhi_solver_analysis(struct sc26_xianzhi_solver *solver);
int sc26_xianzhi_solver_numeric_prepare(struct sc26_xianzhi_solver *solver);
int sc26_xianzhi_solver_solve(struct sc26_xianzhi_solver *solver);

/* Copy the final device solution to input->x/x_im, print statistics, and free
 * all product objects. This is called after the timed loop. */
int sc26_xianzhi_solver_finish(struct sc26_xianzhi_solver *solver);

/* Release an incomplete or already finished context. */
void sc26_xianzhi_solver_destroy(struct sc26_xianzhi_solver *solver);

#ifdef __cplusplus
}
#endif

#endif
