#ifndef SC26_PRODUCT_PANGULU_DIRECT_SOLVER_H
#define SC26_PRODUCT_PANGULU_DIRECT_SOLVER_H

#include "solverchallenge26/solver_input.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sc26_pangulu_direct_solver;

int sc26_pangulu_direct_solver_create(
    struct sc26_pangulu_direct_solver **solver,
    const struct SC26_SolverInput *input);
int sc26_pangulu_direct_solver_analysis(
    struct sc26_pangulu_direct_solver *solver);
int sc26_pangulu_direct_solver_factorize(
    struct sc26_pangulu_direct_solver *solver);
int sc26_pangulu_direct_solver_solve(
    struct sc26_pangulu_direct_solver *solver);
void sc26_pangulu_direct_solver_report(
    struct sc26_pangulu_direct_solver *solver);
int sc26_pangulu_direct_solver_finish(
    struct sc26_pangulu_direct_solver *solver);
void sc26_pangulu_direct_solver_destroy(
    struct sc26_pangulu_direct_solver *solver);

#ifdef __cplusplus
}
#endif

#endif
