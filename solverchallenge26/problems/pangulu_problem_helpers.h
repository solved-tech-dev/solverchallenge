#ifndef SC26_PANGULU_PROBLEM_HELPERS_H
#define SC26_PANGULU_PROBLEM_HELPERS_H

#include "pangulu_problem.h"
#include "product/pangulu_direct_solver.h"

#include <stdio.h>
#include <stdlib.h>

/* Return the PanguLU state stored in the public solver object. */
static struct sc26_pangulu_direct_solver* sc26_pangulu_state(void* solver)
{
    struct MySolver* state = (struct MySolver*)solver;
    return state ? (struct sc26_pangulu_direct_solver*)state->product : NULL;
}

/* Stop the test program when an adapter call fails. */
static void sc26_pangulu_require(int status, const char* operation)
{
    if (!status) {
        return;
    }
    fprintf(stderr, "%s failed: %d\n", operation, status);
    exit(EXIT_FAILURE);
}

void sc26_problem_reset(void* solver)
{
    sc26_pangulu_require(
        sc26_pangulu_direct_solver_reset(sc26_pangulu_state(solver)),
        "PanguLU solution reset");
}

/* Connect the direct program phases to the PanguLU adapter. */
#define SC26_DEFINE_PANGULU_DIRECT_CALLBACKS()                                                                                                       \
    void preprocess(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_analysis(sc26_pangulu_state(solver)), "PanguLU analysis");                                   \
    }                                                                                                                                                \
    void direct_solver(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_factorize(sc26_pangulu_state(solver)), "PanguLU factorization");                             \
    }                                                                                                                                                \
    void solve(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                        \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_solve(sc26_pangulu_state(solver)), "PanguLU solve");                                         \
    }                                                                                                                                                \
    void preprocess_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_analysis(sc26_pangulu_state(solver)), "PanguLU complex analysis");                           \
    }                                                                                                                                                \
    void direct_solver_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_factorize(sc26_pangulu_state(solver)), "PanguLU complex factorization");                     \
    }                                                                                                                                                \
    void solve_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                        \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_solve(sc26_pangulu_state(solver)), "PanguLU complex solve");                                 \
    }

/* Connect the iterative program phases to the PanguLU adapter. */
#define SC26_DEFINE_PANGULU_ITERATIVE_CALLBACKS()                                                                                                    \
    void analyse(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                      \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_analysis(sc26_pangulu_state(solver)), "PanguLU analysis");                                   \
    }                                                                                                                                                \
    void preprocess(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_factorize(sc26_pangulu_state(solver)), "PanguLU factorization");                             \
    }                                                                                                                                                \
    void iterative_solver(struct MySolver* solver, const struct SC26_SolverInput* input)                                                             \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_solve(sc26_pangulu_state(solver)), "PanguLU solve");                                         \
    }                                                                                                                                                \
    void analyse_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                      \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_analysis(sc26_pangulu_state(solver)), "PanguLU complex analysis");                           \
    }                                                                                                                                                \
    void preprocess_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_factorize(sc26_pangulu_state(solver)), "PanguLU complex factorization");                     \
    }                                                                                                                                                \
    void iterative_solver_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                             \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_pangulu_require(sc26_pangulu_direct_solver_solve(sc26_pangulu_state(solver)), "PanguLU complex solve");                                 \
    }

/* Copy the final solution and release the PanguLU adapter. */
#define SC26_DEFINE_PANGULU_DESTROY()                                                                                                                \
    void sc26_problem_report(void* solver)                                                                                                          \
    {                                                                                                                                                \
        struct MySolver*                   state = (struct MySolver*)solver;                                                                         \
        struct sc26_pangulu_direct_solver* product;                                                                                                  \
        if (!state)                                                                                                                                  \
            return;                                                                                                                                  \
        product = (struct sc26_pangulu_direct_solver*)state->product;                                                                                \
        if (product)                                                                                                                                 \
            sc26_pangulu_direct_solver_report(product);                                                                                              \
    }                                                                                                                                                \
    void sc26_problem_destroy(void* solver, const struct SC26_SolverInput* input)                                                                    \
    {                                                                                                                                                \
        struct MySolver*                   state = (struct MySolver*)solver;                                                                         \
        struct sc26_pangulu_direct_solver* product;                                                                                                  \
        (void)input;                                                                                                                                 \
        if (!state)                                                                                                                                  \
            return;                                                                                                                                  \
        product        = (struct sc26_pangulu_direct_solver*)state->product;                                                                         \
        state->product = NULL;                                                                                                                       \
        if (product)                                                                                                                                 \
            sc26_pangulu_require(sc26_pangulu_direct_solver_finish(product), "PanguLU solution copy");                                               \
    }

#endif
