/* SolverChallenge26 problem 04 solver interface. */
#ifndef SC26_PROBLEM04_SOLVER_H
#define SC26_PROBLEM04_SOLVER_H

#include "solverchallenge26/solver_input.h"

struct MySolver {
    /* add solver state here */
};

struct MySolver_complex {
    /* add solver state here */
};

#ifdef DIRECT_SOLVER
void preprocess(struct MySolver* solver, const struct SC26_SolverInput* input);
void direct_solver(struct MySolver* solver, const struct SC26_SolverInput* input);
void solve(struct MySolver* solver, const struct SC26_SolverInput* input);
void preprocess_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input);
void direct_solver_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input);
void solve_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input);
#endif

#ifdef ITERATIVE_SOLVER
void analyse(struct MySolver* solver, const struct SC26_SolverInput* input);
void preprocess(struct MySolver* solver, const struct SC26_SolverInput* input);
void iterative_solver(struct MySolver* solver, const struct SC26_SolverInput* input);
void analyse_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input);
void preprocess_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input);
void iterative_solver_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input);
#endif

#endif
