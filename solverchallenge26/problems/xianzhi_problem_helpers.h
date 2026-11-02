#ifndef SC26_XIANZHI_PROBLEM_HELPERS_H
#define SC26_XIANZHI_PROBLEM_HELPERS_H

#include "product/xianzhi_solver.h"
#include "xianzhi_problem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Return the XianzhiKrylov state stored in the public solver object. */
static struct sc26_xianzhi_solver* sc26_xianzhi_state(void* solver)
{
    struct MySolver* state = (struct MySolver*)solver;
    return state ? (struct sc26_xianzhi_solver*)state->product : NULL;
}

/* Stop the test program when an adapter call fails. */
static void sc26_xianzhi_require(int status, const char* operation)
{
    if (status == 0) {
        return;
    }
    fprintf(stderr, "%s failed: %d\n", operation, status);
    exit(EXIT_FAILURE);
}

/* Build the absolute path of a JSON configuration file in the build tree. */
static void sc26_problem_config_path(char* output, size_t output_size, const char* file_name)
{
#ifndef SC26_PROBLEM_CONFIG_DIR
    (void)output;
    (void)output_size;
    (void)file_name;
    fprintf(stderr, "SC26_PROBLEM_CONFIG_DIR is not defined\n");
    exit(EXIT_FAILURE);
#else
    int written = snprintf(output, output_size, "%s/%s", getenv("SC26_CONFIG_DIR") ? getenv("SC26_CONFIG_DIR") : SC26_PROBLEM_CONFIG_DIR, file_name);
    if (written < 0 || (size_t)written >= output_size) {
        fprintf(stderr, "problem configuration path is too long\n");
        exit(EXIT_FAILURE);
    }
#endif
}

/* Create the shared XianzhiKrylov adapter with problem-specific options. */
static void sc26_problem_create_xianzhi(void* solver, const struct SC26_SolverInput* input, const char* config_path, int complex_values,
                                        int dd_solution, const char* provider_library, const char* provider_config)
{
    struct MySolver*            state = (struct MySolver*)solver;
    struct sc26_xianzhi_options options;
    memset(&options, 0, sizeof(options));
    options.config_template  = config_path;
    options.provider_library = provider_library;
    options.provider_config  = provider_config;
    options.complex_values   = complex_values;
    options.dd_solution      = dd_solution;
    sc26_xianzhi_require(sc26_xianzhi_solver_create((struct sc26_xianzhi_solver**)&state->product, input, &options), "solver creation");
}

/* Connect the iterative program phases to the XianzhiKrylov adapter. */
#define SC26_DEFINE_XIANZHI_ITERATIVE_CALLBACKS()                                                                                                    \
    void analyse(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                      \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_analysis(sc26_xianzhi_state(solver)), "analysis");                                                  \
    }                                                                                                                                                \
    void preprocess(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_numeric_prepare(sc26_xianzhi_state(solver)), "numeric preparation");                                \
    }                                                                                                                                                \
    void iterative_solver(struct MySolver* solver, const struct SC26_SolverInput* input)                                                             \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_solve(sc26_xianzhi_state(solver)), "solve");                                                        \
    }                                                                                                                                                \
    void analyse_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                      \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_analysis(sc26_xianzhi_state(solver)), "complex analysis");                                          \
    }                                                                                                                                                \
    void preprocess_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_numeric_prepare(sc26_xianzhi_state(solver)), "complex numeric preparation");                        \
    }                                                                                                                                                \
    void iterative_solver_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                             \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_solve(sc26_xianzhi_state(solver)), "complex solve");                                                \
    }

/* Connect the direct program phases to the XianzhiKrylov adapter. */
#define SC26_DEFINE_XIANZHI_DIRECT_CALLBACKS()                                                                                                       \
    void preprocess(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_analysis(sc26_xianzhi_state(solver)), "analysis");                                                  \
    }                                                                                                                                                \
    void direct_solver(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_numeric_prepare(sc26_xianzhi_state(solver)), "numeric preparation");                                \
    }                                                                                                                                                \
    void solve(struct MySolver* solver, const struct SC26_SolverInput* input)                                                                        \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_solve(sc26_xianzhi_state(solver)), "solve");                                                        \
    }                                                                                                                                                \
    void preprocess_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                   \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_analysis(sc26_xianzhi_state(solver)), "complex analysis");                                          \
    }                                                                                                                                                \
    void direct_solver_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_numeric_prepare(sc26_xianzhi_state(solver)), "complex numeric preparation");                        \
    }                                                                                                                                                \
    void solve_complex(struct MySolver_complex* solver, const struct SC26_SolverInput* input)                                                        \
    {                                                                                                                                                \
        (void)input;                                                                                                                                 \
        sc26_xianzhi_require(sc26_xianzhi_solver_solve(sc26_xianzhi_state(solver)), "complex solve");                                                \
    }

void sc26_problem_report(void* solver) { sc26_xianzhi_require(sc26_xianzhi_solver_report(sc26_xianzhi_state(solver)), "statistics"); }

/* Report statistics, copy the final solution, and release the adapter. */
#define SC26_DEFINE_XIANZHI_DESTROY()                                                                                                                \
    void sc26_problem_destroy(void* solver, const struct SC26_SolverInput* input)                                                                    \
    {                                                                                                                                                \
        struct MySolver*            state = (struct MySolver*)solver;                                                                                \
        struct sc26_xianzhi_solver* product;                                                                                                         \
        (void)input;                                                                                                                                 \
        if (!state)                                                                                                                                  \
            return;                                                                                                                                  \
        product        = (struct sc26_xianzhi_solver*)state->product;                                                                                \
        state->product = NULL;                                                                                                                       \
        if (product)                                                                                                                                 \
            sc26_xianzhi_require(sc26_xianzhi_solver_finish(product), "solution copy");                                                              \
    }

#endif
