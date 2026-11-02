/*
 * This is the reference MPI direct method solver code of SolverChallenge26
 */

// system file
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "mpi.h"

#include "solverchallenge26/diagnostics.h"
#include "solverchallenge26/mpi_system_loader.h"
#include "solverchallenge26/solver_api.h"

int main(int argc, char** argv)
{

    int     m, n, nnzA, isSymmetricA;
    int*    row_ptr = NULL;         // the csr row pointer array of matrix A
    int*    col_idx = NULL;         // the csr column index array of matrix A
    double* val     = NULL;         // the csr value array of matrix A (real number)
    double* val_im  = NULL;         // the csr value array of matrix A (imaginary number)
    SC26_SolutionScalar *x = NULL, *x_im = NULL; // solution vector x, (x: real number, x_im: imaginary number)
    double *b = NULL, *b_im = NULL; // right-hand side vector b, (b: real number, b_im: imaginary number)
    double  tt, time;
    int local_n, row_begin;
    struct MPI_LocalSystem local_system;
    int *gather_counts = NULL, *gather_displs = NULL;
    SC26_SolutionScalar *x_global = NULL, *x_global_im = NULL;

    char* filename_matrix;       // the filename of matrix A
    char* filename_b;            // the filename of right-hand side vector b
    int   read_matrix_base = 1;  // 0-base or 1-base, default 1-base
    int   test_frequency   = 5; // run code frequency
    int   warmup_frequency = 5; // untimed warm-up runs required by the report protocol
    int   sys_type         = 0;  // type of algebraic systems, 0: real, 1: complex; default 0

    int myrank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    /* ========================================== */
    // Step 0: Read command line argument
    /* ========================================== */
    if (argc < 3 || argc > 5) {
        if (myrank == 0) print_help();
        MPI_Finalize();
        return -1;
    }
    filename_matrix = argv[1];
    filename_b = argv[2];
    if (argc >= 4) {
        filename_matrix  = argv[1];
        filename_b       = argv[2];
        read_matrix_base = atoi(argv[3]);

        if (!((read_matrix_base == 0) || (read_matrix_base == 1))) {
            if (myrank == 0) {
                fprintf(stdout, "read_base error input %d\n", read_matrix_base);
                print_help();
            }
            MPI_Finalize();
            return -1;
        }
    }
    if (argc == 5) {
        sys_type = atoi(argv[4]);
        if (sys_type > 1 || sys_type < 0) {
            if (myrank == 0) {
                fprintf(stdout, "sys_type error input %d\n", sys_type);
                print_help();
            }
            MPI_Finalize();
            return -1;
        }
    }

    //! If the user does not specify parameter sys_type,
    //! it will automatically determine whether it is a real system or a complex system
    if (argc < 5) {
        if (detect_sys_type_from_matrix(filename_matrix, &sys_type) != 0) {
            if (myrank == 0) fprintf(stdout, "Failed to detect system type from %s\n", filename_matrix);
            MPI_Finalize();
            return -1;
        }
    }

    if (myrank == 0)
        fprintf(stdout, "matrix name :      %s\nvectorb name :     %s\nread function :    %s\nsys_type :         %d\n",
                filename_matrix, filename_b, file_is_petsc_matrix(filename_matrix) ? "PETSc binary (0-base)" : (read_matrix_base ? "MTX base-1" : "MTX base-0"),
                sys_type);

    /* ========================================== */
    // Step 1: Load matrix and rhs (PETSc binary by default)
    /* ========================================== */
    if (mpi_load_local_linear_system(filename_matrix, filename_b, read_matrix_base, sys_type,
                                     MPI_COMM_WORLD, &local_system) != 0) {
        MPI_Finalize();
        return -1;
    }
    m = local_system.global_rows;
    n = local_system.global_cols;
    nnzA = local_system.global_nnz;
    row_begin = local_system.row_begin;
    local_n = local_system.row_end - local_system.row_begin;
    row_ptr = local_system.row_ptr;
    col_idx = local_system.col_idx;
    val = local_system.val;
    val_im = local_system.val_im;
    b = local_system.b;
    b_im = local_system.b_im;
    x = local_n ? (SC26_SolutionScalar*)calloc((size_t)local_n, sizeof(SC26_SolutionScalar)) : NULL;
    int allocation_status = local_n && !x ? -1 : 0;
    if (sys_type) {
        x_im = local_n ? (SC26_SolutionScalar*)calloc((size_t)local_n, sizeof(SC26_SolutionScalar)) : NULL;
        if (local_n && !x_im) allocation_status = -1;
    }
    int global_allocation_status = 0;
    MPI_Allreduce(&allocation_status, &global_allocation_status, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (global_allocation_status) {
        free(x);
        free(x_im);
        mpi_local_system_free(&local_system);
        MPI_Finalize();
        return -1;
    }

    struct SC26_SolverInput solver_input = {
        .global_n = n,
        .local_n = local_n,
        .row_begin = row_begin,
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
        .b_im_all = b_im,
        .comm = MPI_COMM_WORLD
    };

    /* ========================================== */
    // Step 2: Solve the linear system
    /* ========================================== */
    if (sys_type == 0) {
        struct MySolver mysolver;
        SC26_SOLVER_CREATE(&mysolver, &solver_input);
        for (int i = 0; i < warmup_frequency; ++i) {
            preprocess(&mysolver, &solver_input);
            direct_solver(&mysolver, &solver_input);
            solve(&mysolver, &solver_input);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tt = MPI_Wtime();
        for (int i = 0; i < test_frequency; ++i) {
            preprocess(&mysolver, &solver_input);
            direct_solver(&mysolver, &solver_input);
            solve(&mysolver, &solver_input);
        }
        time = (MPI_Wtime() - tt) * 1000.0 / (double)test_frequency;
        SC26_SOLVER_DESTROY(&mysolver, &solver_input);
    } else {
        struct MySolver_complex mysolver;
        SC26_SOLVER_CREATE(&mysolver, &solver_input);
        for (int i = 0; i < warmup_frequency; ++i) {
            preprocess_complex(&mysolver, &solver_input);
            direct_solver_complex(&mysolver, &solver_input);
            solve_complex(&mysolver, &solver_input);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        tt = MPI_Wtime();
        for (int i = 0; i < test_frequency; ++i) {
            preprocess_complex(&mysolver, &solver_input);
            direct_solver_complex(&mysolver, &solver_input);
            solve_complex(&mysolver, &solver_input);
        }
        time = (MPI_Wtime() - tt) * 1000.0 / (double)test_frequency;
        SC26_SOLVER_DESTROY(&mysolver, &solver_input);
    }
    {
        double local_time_ms = time;
        MPI_Reduce(&local_time_ms, &time, 1, MPI_DOUBLE, MPI_MAX, 0,
                   MPI_COMM_WORLD);
    }

    /* ========================================== */
    // Step 3: Check time, memory and correctness
    /* ========================================== */
    // check the memory
    mem_usage();

    gather_counts = (int*)malloc((size_t)size * sizeof(*gather_counts));
    gather_displs = (int*)malloc((size_t)size * sizeof(*gather_displs));
    int gather_array_status = (!gather_counts || !gather_displs) ? -1 : 0;
    int global_gather_array_status = 0;
    MPI_Allreduce(&gather_array_status, &global_gather_array_status, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    if (global_gather_array_status) {
        if (myrank == 0) fprintf(stdout, "MPI solution gather allocation failed.\n");
        free(gather_counts);
        free(gather_displs);
        free(x);
        free(x_im);
        mpi_local_system_free(&local_system);
        MPI_Finalize();
        return -1;
    }
    mpi_row_counts(n, size, gather_counts, gather_displs);
    int gather_status = 0;
    if (myrank == 0) {
        x_global = (SC26_SolutionScalar*)malloc(sizeof(SC26_SolutionScalar) * (size_t)n);
        if (sys_type) x_global_im = (SC26_SolutionScalar*)malloc(sizeof(SC26_SolutionScalar) * (size_t)n);
        if (!x_global || (sys_type && !x_global_im)) gather_status = -1;
    }
    MPI_Bcast(&gather_status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (gather_status) {
        free(x_global);
        free(x_global_im);
        free(x);
        free(x_im);
        mpi_local_system_free(&local_system);
        free(gather_counts);
        free(gather_displs);
        MPI_Finalize();
        return -1;
    }
    MPI_Datatype solution_type = MPI_DOUBLE;
#ifdef SC26_SOLUTION_FP128
    MPI_Type_contiguous(sizeof(SC26_SolutionScalar), MPI_BYTE, &solution_type);
    MPI_Type_commit(&solution_type);
#endif
    MPI_Gatherv(x, local_n, solution_type, x_global, gather_counts, gather_displs,
                solution_type, 0, MPI_COMM_WORLD);
    if (sys_type)
        MPI_Gatherv(x_im, local_n, solution_type, x_global_im, gather_counts, gather_displs,
                    solution_type, 0, MPI_COMM_WORLD);
#ifdef SC26_SOLUTION_FP128
    MPI_Type_free(&solution_type);
#endif
    free(x);
    free(x_im);
    x = NULL;
    x_im = NULL;
    mpi_local_system_free(&local_system);
    row_ptr = NULL;
    col_idx = NULL;
    val = NULL;
    val_im = NULL;
    b = NULL;
    b_im = NULL;
    free(gather_counts);
    free(gather_displs);

    /* Re-read the complete system only on rank 0 for the final residual check. */
    int reload_status = 0;
    if (myrank == 0) {
        reload_status = load_linear_system(filename_matrix, filename_b, read_matrix_base, sys_type,
                                           &m, &n, &nnzA, &isSymmetricA, &row_ptr, &col_idx,
                                           &val, &val_im, &b, &b_im);
        if (reload_status == 0 && m != n) reload_status = -1;
    }
    MPI_Bcast(&reload_status, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (myrank == 0) {

        fprintf(stdout, "------------------------------------------\n");

        // check the memory
        // mem_usage();

        // Print the average measured solve time.
        fprintf(stdout, "time_ms=%.6lf\n", time);

        // store x to a file
        // promote the solution to long double for output and high-precision checks
        long double* x_ld = (long double*)malloc(sizeof(long double) * n);
        for (int i = 0; i < n; i++) x_ld[i] = (long double)x_global[i];
        long double* x_im_ld = NULL;
        if (sys_type) {
            x_im_ld = (long double*)malloc(sizeof(long double) * n);
            for (int i = 0; i < n; i++) x_im_ld[i] = (long double)x_global_im[i];
        }
#ifndef SC26_DISABLE_FLOAT128
        __float128* x_q = (__float128*)malloc(sizeof(__float128) * n);
        for (int i = 0; i < n; i++) x_q[i] = (__float128)x_global[i];
        __float128* x_q_im = NULL;
        if (sys_type) {
            x_q_im = (__float128*)malloc(sizeof(__float128) * n);
            for (int i = 0; i < n; i++) x_q_im[i] = (__float128)x_global_im[i];
        }
#endif

        char* answer_x = "answer_x.rhs";
        if (sys_type == 0) // real rhs
            store_x_ld(n, x_ld, answer_x);
        else // complex rhs
            store_x_complex_ld(n, x_ld, x_im_ld, answer_x);

        // check the correctness
        if (reload_status != 0) {
            fprintf(stdout, "Final full-system reload failed; residual check skipped.\n");
        } else if (sys_type == 0) // real system
        {
            // 1) using double precision
#ifndef SC26_SOLUTION_FP128
            check_correctness(n, row_ptr, col_idx, val, x_global, b);
#endif
            // 2) long double solution, long double evaluation
#ifndef SC26_SOLUTION_FP128
            check_correctness_ld_xld(n, row_ptr, col_idx, val, x_ld, b);
#endif
#ifndef SC26_DISABLE_FLOAT128
            // 3) float128 solution, float128 evaluation
            check_correctness_q_xq(n, row_ptr, col_idx, val, x_q, b);
#endif

        } else { // complex system
            // 1) using double precision
#ifndef SC26_SOLUTION_FP128
            check_correctness_complex(n, row_ptr, col_idx, val, val_im, x_global, x_global_im, b, b_im);
#endif
            // 2) long double solution, long double evaluation
#ifndef SC26_SOLUTION_FP128
            check_correctness_complex_ld_xld(n, row_ptr, col_idx, val, val_im, x_ld, x_im_ld, b, b_im);
#endif
#ifndef SC26_DISABLE_FLOAT128
            // 3) float128 solution, float128 evaluation
            check_correctness_complex_q_xq(n, row_ptr, col_idx, val, val_im, x_q, x_q_im, b, b_im);
#endif

        }
        free(val_im);
        free(b_im);
        free(x_global);
        free(x_global_im);
        free(x_im_ld);
        free(x_ld);
#ifndef SC26_DISABLE_FLOAT128
        free(x_q_im);
        free(x_q);
#endif
        free(row_ptr);
        free(col_idx);
        free(val);
        free(b);
    }

    MPI_Finalize();
    return 0;
}
