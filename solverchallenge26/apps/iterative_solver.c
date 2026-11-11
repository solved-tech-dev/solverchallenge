/*
 * This is the reference iterative method solver code of SolverChallenge26
 */

// system file
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "solverchallenge26/diagnostics.h"
#include "solverchallenge26/system_loader.h"
#include "solverchallenge26/solver_api.h"
#include "solverchallenge26/rounds.h"

int main(int argc, char** argv)
{
    int     m, n, nnzA, isSymmetricA;
    int*    row_ptr = NULL;         // the csr row pointer array of matrix A
    int*    col_idx = NULL;         // the csr column index array of matrix A
    double* val     = NULL;         // the csr value array of matrix A (real number)
    double* val_im  = NULL;         // the csr value array of matrix A (imaginary number)
    SC26_SolutionScalar *x = NULL, *x_im = NULL; // solution vector x, (x: real number, x_im: imaginary number)
    double *b = NULL, *b_im = NULL; // right-hand side vector b, (b: real number, b_im: imaginary number)
    double  tt;

    char* filename_matrix;       // the filename of matrix A
    char* filename_b;            // the filename of right-hand side vector b
    int   read_matrix_base = 1;  // 0-base or 1-base, default 1-base
    int   sys_type         = 0;  // type of algebraic systems, 0: real, 1: complex; default 0

    /* ========================================== */
    // Step 0: Read command line argument
    /* ========================================== */
    if (argc < 3 || argc > 5) {
        print_help();
        return -1;
    }
    filename_matrix = argv[1];
    filename_b = argv[2];
    if (argc >= 4) {
        filename_matrix  = argv[1];
        filename_b       = argv[2];
        read_matrix_base = atoi(argv[3]);
        if (!((read_matrix_base == 0) || (read_matrix_base == 1))) {
            fprintf(stdout, "read_base error input %d\n", read_matrix_base);
            print_help();
            return -1;
        }
    }
    if (argc == 5) {
        sys_type = atoi(argv[4]);
        if (sys_type > 1 || sys_type < 0) {
            fprintf(stdout, "sys_type error input %d\n", sys_type);
            print_help();
            return -1;
        }
    }

    //! If the user does not specify parameter sys_type,
    //! it will automatically determine whether it is a real system or a complex system
    if (argc < 5) {
        if (detect_sys_type_from_matrix(filename_matrix, &sys_type) != 0) {
            fprintf(stdout, "Failed to detect system type from %s\n", filename_matrix);
            return -1;
        }
    }

    fprintf(stdout, "matrix name :      %s\nvectorb name :     %s\nread function :    %s\nsys_type :         %d\n",
            filename_matrix, filename_b, file_is_petsc_matrix(filename_matrix) ? "PETSc binary (0-base)" : (read_matrix_base ? "MTX base-1" : "MTX base-0"),
            sys_type);

    /* ========================================== */
    // Step 1: Load matrix and rhs (PETSc binary by default)
    /* ========================================== */
    if (load_linear_system(filename_matrix, filename_b, read_matrix_base, sys_type, &m, &n, &nnzA, &isSymmetricA, &row_ptr, &col_idx, &val, &val_im, &b,
                           &b_im) != 0) {
        return -1;
    }
    if (m != n) {
        fprintf(stdout, "Invalid matrix size.\n");
        return 1;
    }
    /* Coordinate sorting is input preparation, before any solver timing. */
    if (!strcmp(SC26_PROBLEM_ID, "05") && sc26_sort_csr_rows(n,row_ptr,col_idx,val,val_im))
        return 1;

    x = (SC26_SolutionScalar*)calloc((size_t)n, sizeof(SC26_SolutionScalar));
    if (sys_type) {
        x_im = (SC26_SolutionScalar*)calloc((size_t)n, sizeof(SC26_SolutionScalar));
    }

    struct SC26_SolverInput solver_input = {
        .global_n = n,
        .local_n = n,
        .row_begin = 0,
        .rhs_count = 1,
        .row_ptr = row_ptr,
        .col_idx = col_idx,
        .val = val,
        .val_im = val_im,
        .x = x,
        .x_im = x_im,
        .b = b,
        .b_im = b_im,
        .x_all = x,
        .x_im_all = x_im,
        .b_all = b,
        .b_im_all = b_im
    };

    /* ========================================== */
    // Step 2: Solve the linear system
    /* ========================================== */
    if (sys_type == 0) {
        /* Each round owns a fresh solver context and starts from x = 0. */
        for (int trial = sc26_first_round(); trial < sc26_round_limit(); ++trial) {
            struct MySolver mysolver;
            memset(x, 0, sizeof(*x) * (size_t)n);
            SC26_SOLVER_CREATE(&mysolver, &solver_input);
            tt = GetCurrentTime();
            analyse(&mysolver, &solver_input);
            preprocess(&mysolver, &solver_input);
            iterative_solver(&mysolver, &solver_input);
            double elapsed = GetCurrentTime() - tt;
            printf("TRIAL trial=%d raw_seconds=%.12f\n", trial, elapsed / 1000.0);
            SC26_SOLVER_REPORT(&mysolver);
            SC26_SOLVER_DESTROY(&mysolver, &solver_input);
        }
    }
    else {
        for (int trial = sc26_first_round(); trial < sc26_round_limit(); ++trial) {
            struct MySolver_complex mysolver;
            memset(x, 0, sizeof(*x) * (size_t)n);
            memset(x_im, 0, sizeof(*x_im) * (size_t)n);
            SC26_SOLVER_CREATE(&mysolver, &solver_input);
            tt = GetCurrentTime();
            analyse_complex(&mysolver, &solver_input);
            preprocess_complex(&mysolver, &solver_input);
            iterative_solver_complex(&mysolver, &solver_input);
            double elapsed = GetCurrentTime() - tt;
            printf("TRIAL trial=%d raw_seconds=%.12f\n", trial, elapsed / 1000.0);
            SC26_SOLVER_REPORT(&mysolver);
            SC26_SOLVER_DESTROY(&mysolver, &solver_input);
        }
    }

    fprintf(stdout, "------------------------------------------\n");

    /* ========================================== */
    // Step 3: Check memory and correctness
    /* ========================================== */

    // check the memory
    mem_usage();

    // store x to a file
    // promote the solution to long double for output and high-precision checks
    long double* x_ld = (long double*)malloc(sizeof(long double) * n);
    for (int i = 0; i < n; i++) x_ld[i] = (long double)x[i];
    long double* x_im_ld = NULL;
    if (sys_type) {
        x_im_ld = (long double*)malloc(sizeof(long double) * n);
        for (int i = 0; i < n; i++) x_im_ld[i] = (long double)x_im[i];
    }
#ifndef SC26_DISABLE_FLOAT128
    __float128* x_q = (__float128*)malloc(sizeof(__float128) * n);
    for (int i = 0; i < n; i++) x_q[i] = (__float128)x[i];
    __float128* x_q_im = NULL;
    if (sys_type) {
        x_q_im = (__float128*)malloc(sizeof(__float128) * n);
        for (int i = 0; i < n; i++) x_q_im[i] = (__float128)x_im[i];
    }
#endif

    char* answer_x = "answer_x.rhs";
    if (sys_type == 0) // real rhs
        store_x_ld(n, x_ld, answer_x);
    else // complex rhs
        store_x_complex_ld(n, x_ld, x_im_ld, answer_x);

    long double verified_residual = INFINITY;
    // check the correctness
    if (sys_type == 0) // real system
    {
        // 1) using double precision
#ifndef SC26_SOLUTION_FP128
        check_correctness(n, row_ptr, col_idx, val, x, b);
#endif
        // 2) long double solution, long double evaluation
#ifndef SC26_SOLUTION_FP128
        check_correctness_ld_xld(n, row_ptr, col_idx, val, x_ld, b);
#endif
#ifndef SC26_DISABLE_FLOAT128
        // 3) float128 solution, float128 evaluation
        verified_residual = check_correctness_q_xq(n, row_ptr, col_idx, val, x_q, b);
#endif

    } else { // complex system
        // 1) using double precision
#ifndef SC26_SOLUTION_FP128
        check_correctness_complex(n, row_ptr, col_idx, val, val_im, x, x_im, b, b_im);
#endif
        // 2) long double solution, long double evaluation
#ifndef SC26_SOLUTION_FP128
        check_correctness_complex_ld_xld(n, row_ptr, col_idx, val, val_im, x_ld, x_im_ld, b, b_im);
#endif
#ifndef SC26_DISABLE_FLOAT128
        // 3) float128 solution, float128 evaluation
        verified_residual = check_correctness_complex_q_xq(n, row_ptr, col_idx, val, val_im, x_q, x_q_im, b, b_im);
#endif

        free(val_im);
        free(x_im);
        free(x_im_ld);
#ifndef SC26_DISABLE_FLOAT128
        free(x_q_im);
#endif
        free(b_im);
    }

    free(row_ptr);
    free(col_idx);
    free(val);
    free(x_ld);
#ifndef SC26_DISABLE_FLOAT128
    free(x_q);
#endif
    free(x);
    free(b);

    long double limit = 1e-8L;
    const char *case_name = getenv("SC26_CASE");
    int q2 = case_name && !strcmp(case_name,"q2");
    if (!strcmp(SC26_PROBLEM_ID, "01")) {
        limit = q2 ? 1e-8L : 1e-4L;
    }
    if (!strcmp(SC26_PROBLEM_ID, "02")) {
        limit = q2 ? 1e-10L : 1e-6L;
    }
    if (!strcmp(SC26_PROBLEM_ID, "07")) {
        limit = 1e-6L;
    }
    int passed=isfinite(verified_residual) && verified_residual<limit;
    printf("VERIFICATION relative_l2=%.12Le threshold=%.1Le %s\n",
           verified_residual,limit,passed?"PASS":"FAIL");
    return passed ? 0 : 1;
}
