#include "multi_rhs_common.h"
#include "solverchallenge26/solver_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv)
{
    struct sc26_multi_system system;
    struct SC26_SolverInput input;
    char* residual_mode;
    double tolerance;
    int is_complex;
    double start = 0.0;
    double seconds = 0.0;
    int pass;
    if (argc < 5) {
        fprintf(
            stderr,
            "usage: %s MATRIX SYS_TYPE TOLERANCE RHS [RHS ...]\n",
            argv[0]);
        return EXIT_FAILURE;
    }
    is_complex = atoi(argv[2]);
    tolerance = strtod(argv[3], NULL);
    if ((is_complex != 0 && is_complex != 1) || !(tolerance > 0.0))
        return EXIT_FAILURE;
    residual_mode = getenv("SC26_RESIDUAL_MODE");
    sc26_multi_load(
        &system, argv[1], is_complex, argc - 4, argv + 4);
    sc26_multi_input(&input, &system);
    if (is_complex) {
        struct MySolver_complex solver;
        SC26_SOLVER_CREATE(&solver, &input);
        for (int trial = 0; trial < 10; ++trial) {
            start = sc26_multi_seconds();
            preprocess_complex(&solver, &input);
            direct_solver_complex(&solver, &input);
            solve_complex(&solver, &input);
            double elapsed = sc26_multi_seconds() - start;
            if (trial >= 5) seconds += elapsed;
            printf("TRIAL trial=%d raw_seconds=%.12f\n", trial, elapsed);
        }
        SC26_SOLVER_DESTROY(&solver, &input);
    } else {
        struct MySolver solver;
        SC26_SOLVER_CREATE(&solver, &input);
        for (int trial = 0; trial < 10; ++trial) {
            start = sc26_multi_seconds();
            preprocess(&solver, &input);
            direct_solver(&solver, &input);
            solve(&solver, &input);
            double elapsed = sc26_multi_seconds() - start;
            if (trial >= 5) seconds += elapsed;
            printf("TRIAL trial=%d raw_seconds=%.12f\n", trial, elapsed);
        }
        SC26_SOLVER_DESTROY(&solver, &input);
    }
    pass = sc26_multi_verify(&system, residual_mode, tolerance);
    printf("CHECK end to end time : %12.6f ms\n", seconds * 1000.0 / 5.0);
    printf("COMPETITION_TIME_SECONDS=%.12f\n", seconds / 5.0);
    sc26_multi_free(&system);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
