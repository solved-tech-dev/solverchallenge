#include "multi_rhs_common.h"
#include "solverchallenge26/diagnostics.h"
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
    int pass;

    /*
     * Arguments:
     *   argv[1]  matrix file
     *   argv[2]  0 for a real system, 1 for a complex system
     *   argv[3]  residual tolerance
     *   argv[4+] right-hand-side files
     *
     * The first five rounds warm up the product.  The last five rounds are
     * measured. Initialization and descriptors are created once; every round
     * starts from x = 0 and reruns structure analysis, numeric preparation and
     * solve without retaining previous-round results.
     */
    if (argc < 5) {
        fprintf(
            stderr,
            "usage: %s MATRIX SYS_TYPE TOLERANCE RHS [RHS ...]\n",
            argv[0]);
        return EXIT_FAILURE;
    }
    is_complex = atoi(argv[2]);
    tolerance = strtod(argv[3], NULL);
    if ((is_complex != 0 && is_complex != 1) || !(tolerance > 0.0)) {
        return EXIT_FAILURE;
    }

    residual_mode = getenv("SC26_RESIDUAL_MODE");
    sc26_multi_load(&system, argv[1], is_complex, argc - 4, argv + 4);
    sc26_multi_input(&input, &system);

    if (is_complex) {
        struct MySolver_complex solver;
        SC26_SOLVER_CREATE(&solver, &input);
        for (int trial = 0; trial < 10; ++trial) {
            memset(system.x_all, 0,
                   sizeof(*system.x_all) * (size_t)system.n * (size_t)system.rhs_count);
            memset(system.x_im_all, 0,
                   sizeof(*system.x_im_all) * (size_t)system.n * (size_t)system.rhs_count);
            start = sc26_multi_seconds();
            analyse_complex(&solver, &input);
            preprocess_complex(&solver, &input);
            iterative_solver_complex(&solver, &input);
            double elapsed = sc26_multi_seconds() - start;
            printf("TRIAL trial=%d raw_seconds=%.12f\n", trial, elapsed);
            SC26_SOLVER_REPORT(&solver);
        }
        SC26_SOLVER_DESTROY(&solver, &input);
    } else {
        struct MySolver solver;
        SC26_SOLVER_CREATE(&solver, &input);
        for (int trial = 0; trial < 10; ++trial) {
            memset(system.x_all, 0,
                   sizeof(*system.x_all) * (size_t)system.n * (size_t)system.rhs_count);
            start = sc26_multi_seconds();
            analyse(&solver, &input);
            preprocess(&solver, &input);
            iterative_solver(&solver, &input);
            double elapsed = sc26_multi_seconds() - start;
            printf("TRIAL trial=%d raw_seconds=%.12f\n", trial, elapsed);
            SC26_SOLVER_REPORT(&solver);
        }
        SC26_SOLVER_DESTROY(&solver, &input);
    }

    mem_usage();
    pass = sc26_multi_verify(&system, residual_mode, tolerance);
    sc26_multi_free(&system);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
