#ifndef SC26_PANGULU_PROBLEM_H
#define SC26_PANGULU_PROBLEM_H

#include "solverchallenge26/solver_input.h"

struct MySolver {
    void *product;
};

struct MySolver_complex {
    void *product;
};

void sc26_problem_create(void *solver, const struct SC26_SolverInput *input);
void sc26_problem_report(void *solver);
void sc26_problem_destroy(void *solver, const struct SC26_SolverInput *input);

#define SC26_SOLVER_REPORT(solver) sc26_problem_report((void *)(solver))

#define SC26_SOLVER_CREATE(solver, input) \
    sc26_problem_create((void *)(solver), (input))
#define SC26_SOLVER_DESTROY(solver, input) \
    sc26_problem_destroy((void *)(solver), (input))

#ifdef DIRECT_SOLVER
void preprocess(struct MySolver *solver, const struct SC26_SolverInput *input);
void direct_solver(struct MySolver *solver, const struct SC26_SolverInput *input);
void solve(struct MySolver *solver, const struct SC26_SolverInput *input);
void preprocess_complex(struct MySolver_complex *solver,
                        const struct SC26_SolverInput *input);
void direct_solver_complex(struct MySolver_complex *solver,
                           const struct SC26_SolverInput *input);
void solve_complex(struct MySolver_complex *solver,
                   const struct SC26_SolverInput *input);
#endif

#ifdef ITERATIVE_SOLVER
void analyse(struct MySolver *solver, const struct SC26_SolverInput *input);
void preprocess(struct MySolver *solver, const struct SC26_SolverInput *input);
void iterative_solver(struct MySolver *solver,
                      const struct SC26_SolverInput *input);
void analyse_complex(struct MySolver_complex *solver,
                     const struct SC26_SolverInput *input);
void preprocess_complex(struct MySolver_complex *solver,
                        const struct SC26_SolverInput *input);
void iterative_solver_complex(struct MySolver_complex *solver,
                              const struct SC26_SolverInput *input);
#endif

#endif
