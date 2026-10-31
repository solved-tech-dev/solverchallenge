/* MPI GPU direct-solver harness. The selected problem supplies the adapter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"
#include <cuda_runtime_api.h>

#include "solverchallenge26/diagnostics.h"
#include "solverchallenge26/gpu_direct_solver_api.h"
#include "solverchallenge26/mpi_system_loader.h"

static int parse_args(int argc, char **argv, int rank, char **matrix,
                      char **rhs, int *read_base, int *sys_type)
{
    if (argc < 3 || argc > 5) {
        if (rank == 0) print_help();
        return -1;
    }

    *matrix = argv[1];
    *rhs = argv[2];
    if (argc >= 4) {
        *read_base = atoi(argv[3]);
        if (*read_base != 0 && *read_base != 1) {
            if (rank == 0) fprintf(stderr, "read_base must be 0 or 1\n");
            return -1;
        }
    }
    if (argc == 5) {
        *sys_type = atoi(argv[4]);
        if (*sys_type != 0 && *sys_type != 1) {
            if (rank == 0) fprintf(stderr, "sys_type must be 0 or 1\n");
            return -1;
        }
    } else if (detect_sys_type_from_matrix(*matrix, sys_type) != 0) {
        if (rank == 0)
            fprintf(stderr, "Failed to detect system type from %s\n", *matrix);
        return -1;
    }
    return 0;
}

static void free_global_solution(int **counts, int **displs, SC26_SolutionScalar **x,
                                  SC26_SolutionScalar **x_im)
{
    free(*counts);
    free(*displs);
    free(*x);
    free(*x_im);
    *counts = NULL;
    *displs = NULL;
    *x = NULL;
    *x_im = NULL;
}

int main(int argc, char **argv)
{
    int rank, size;
    int read_base = 1, sys_type = 0;
    char *matrix = NULL, *rhs = NULL;
    struct MPI_LocalSystem local;
    struct SC26_SolverInput input = {0};
    int *counts = NULL, *displs = NULL;
    SC26_SolutionScalar *x_global = NULL, *x_global_im = NULL;
    int status = 0;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (parse_args(argc, argv, rank, &matrix, &rhs, &read_base, &sys_type) !=
        0) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    mpi_local_system_init(&local);
    if (mpi_load_local_linear_system(matrix, rhs, read_base, sys_type,
                                     MPI_COMM_WORLD, &local) != 0) {
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    input.global_n = local.global_cols;
    input.local_n = local.row_end - local.row_begin;
    input.row_begin = local.row_begin;
    input.rhs_count = 1;
    input.row_ptr = local.row_ptr;
    input.col_idx = local.col_idx;
    input.val = local.val;
    input.val_im = local.val_im;
    input.x = input.local_n ? calloc((size_t)input.local_n, sizeof(*input.x))
                            : NULL;
    input.x_im = sys_type && input.local_n
                     ? calloc((size_t)input.local_n, sizeof(*input.x_im))
                     : NULL;
    input.b = local.b;
    input.b_im = local.b_im;
    input.x_all = input.x;
    input.x_im_all = input.x_im;
    input.b_all = input.b;
    input.b_im_all = input.b_im;
    input.comm = MPI_COMM_WORLD;

    status = (input.local_n && !input.x) ||
             (sys_type && input.local_n && !input.x_im);
    {
        int global_status = 0;
        MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX,
                      MPI_COMM_WORLD);
        status = global_status;
    }
    if (status) {
        free(input.x);
        free(input.x_im);
        mpi_local_system_free(&local);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    if (rank == 0) {
        fprintf(stdout,
                "matrix name :      %s\nvectorb name :     %s\nread function :    %s\nsys_type :         %d\n",
                matrix, rhs,
                file_is_petsc_matrix(matrix)
                    ? "PETSc binary (0-base)"
                    : (read_base ? "MTX base-1" : "MTX base-0"),
                sys_type);
    }

    const int warmup_frequency = 5;
    const int test_frequency = 5;
    double local_total_ms = 0.0;
    double local_upload_ms = 0.0;
    double local_download_ms = 0.0;
    unsigned long long local_gpu_tracked_peak = 0;
    unsigned long long local_gpu_observed_peak = 0;
    struct SC26_GpuDirectSolver *solver = NULL;
    struct SC26_GpuMemoryMetrics memory_metrics = {0, 0};
    int global_status = 0;
    status = sc26_gpu_direct_solver_create(&input, &solver);
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    double upload_start = MPI_Wtime();
    if (!status) status = sc26_gpu_direct_solver_upload(solver);
    local_upload_ms = (MPI_Wtime() - upload_start) * 1000.0;
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    for (int trial = 0; trial < warmup_frequency && !status; ++trial)
        status = sc26_gpu_direct_solver_solve(solver);
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    if (!status && cudaDeviceSynchronize() != cudaSuccess)
        status = SC26_GPU_ERR_ARGUMENT;
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    MPI_Barrier(MPI_COMM_WORLD);
    double solve_start = MPI_Wtime();
    for (int trial = 0; trial < test_frequency && !status; ++trial)
        status = sc26_gpu_direct_solver_solve(solver);
    if (!status && cudaDeviceSynchronize() != cudaSuccess)
        status = SC26_GPU_ERR_ARGUMENT;
    local_total_ms = (MPI_Wtime() - solve_start) * 1000.0;
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    double copy_start = MPI_Wtime();
    if (!status) status = sc26_gpu_direct_solver_copy_solution(solver);
    local_download_ms = (MPI_Wtime() - copy_start) * 1000.0;
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    if (!status) status = sc26_gpu_direct_solver_get_memory_metrics(solver, &memory_metrics);
    local_gpu_tracked_peak = memory_metrics.tracked_peak_bytes;
    local_gpu_observed_peak = memory_metrics.observed_peak_bytes;
    if (solver) sc26_gpu_direct_solver_destroy(solver);
    MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    status = global_status;
    {
        double local_ms = local_total_ms / (double)test_frequency;
        double local_upload = local_upload_ms;
        double local_download = local_download_ms;
        double global_ms = 0.0;
        double global_upload = 0.0;
        double global_download = 0.0;
        unsigned long long global_gpu_tracked_peak = 0;
        unsigned long long global_gpu_observed_peak = 0;
        MPI_Reduce(&local_ms, &global_ms, 1, MPI_DOUBLE, MPI_MAX, 0,
                   MPI_COMM_WORLD);
        MPI_Reduce(&local_upload, &global_upload, 1, MPI_DOUBLE, MPI_MAX, 0,
                   MPI_COMM_WORLD);
        MPI_Reduce(&local_download, &global_download, 1, MPI_DOUBLE, MPI_MAX,
                   0, MPI_COMM_WORLD);
        MPI_Reduce(&local_gpu_tracked_peak, &global_gpu_tracked_peak, 1,
                   MPI_UNSIGNED_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);
        MPI_Reduce(&local_gpu_observed_peak, &global_gpu_observed_peak, 1,
                   MPI_UNSIGNED_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);
        if (rank == 0 && !status) {
            printf("------------------------------------------\n");
            printf("GPU memory usage:                   %12.4f MB\n",
                   (double)global_gpu_tracked_peak / (1024.0 * 1024.0));
            printf("GPU observed memory delta:         %12.4f MB\n",
                   (double)global_gpu_observed_peak / (1024.0 * 1024.0));
            printf("CHECK MPI GPU CPU-to-GPU transfer time : %12.6lf ms\n",
                   global_upload);
            printf("CHECK MPI GPU GPU-to-CPU transfer time : %12.6lf ms\n",
                   global_download);
            printf("CHECK MPI GPU end to end time : %12.6lf ms\n", global_ms);
        }
    }

    if (status) {
        if (rank == 0) fprintf(stderr, "MPI GPU solve failed\n");
        free(input.x);
        free(input.x_im);
        mpi_local_system_free(&local);
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    mem_usage();
    counts = malloc(sizeof(*counts) * (size_t)size);
    displs = malloc(sizeof(*displs) * (size_t)size);
    status = (!counts || !displs) ? 1 : 0;
    {
        int global_status = 0;
        MPI_Allreduce(&status, &global_status, 1, MPI_INT, MPI_MAX,
                      MPI_COMM_WORLD);
        status = global_status;
    }
    if (!status) mpi_row_counts(input.global_n, size, counts, displs);
    if (rank == 0 && !status) {
        x_global = malloc(sizeof(*x_global) * (size_t)input.global_n);
        if (sys_type)
            x_global_im = malloc(sizeof(*x_global_im) * (size_t)input.global_n);
        status = !x_global || (sys_type && !x_global_im);
    }
    MPI_Bcast(&status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Datatype solution_type = MPI_DOUBLE;
#ifdef SC26_SOLUTION_FP128
    MPI_Type_contiguous(sizeof(SC26_SolutionScalar), MPI_BYTE, &solution_type);
    MPI_Type_commit(&solution_type);
#endif
    if (!status) {
        MPI_Gatherv(input.x, input.local_n, solution_type, x_global, counts,
                    displs, solution_type, 0, MPI_COMM_WORLD);
        if (sys_type)
            MPI_Gatherv(input.x_im, input.local_n, solution_type, x_global_im,
                        counts, displs, solution_type, 0, MPI_COMM_WORLD);
    }

#ifdef SC26_SOLUTION_FP128
    MPI_Type_free(&solution_type);
#endif
    free(input.x);
    free(input.x_im);
    mpi_local_system_free(&local);

    int reload_status = 0;
    if (rank == 0 && !status) {
        int m, n, nnz, symmetric;
        int *row_ptr = NULL, *col_idx = NULL;
        double *val = NULL, *val_im = NULL, *b = NULL, *b_im = NULL;
        reload_status = load_linear_system(
            matrix, rhs, read_base, sys_type, &m, &n, &nnz, &symmetric,
            &row_ptr, &col_idx, &val, &val_im, &b, &b_im);
        if (reload_status == 0 && m != n) reload_status = 1;
        if (reload_status != 0) {
            fprintf(stderr, "Final full-system reload failed\n");
        } else {
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
            if (sys_type == 0) {
                store_x_ld(n, x_ld, "answer_x.rhs");
                // 1) double solution, double evaluation
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
            } else {
                store_x_complex_ld(n, x_ld, x_im_ld, "answer_x.rhs");
                // 1) double solution, double evaluation
#ifndef SC26_SOLUTION_FP128
                check_correctness_complex(n, row_ptr, col_idx, val, val_im,
                                          x_global, x_global_im, b, b_im);
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
            free(x_im_ld);
            free(x_ld);
#ifndef SC26_DISABLE_FLOAT128
            free(x_q_im);
            free(x_q);
#endif
        }
        free(row_ptr);
        free(col_idx);
        free(val);
        free(val_im);
        free(b);
        free(b_im);
    }
    MPI_Bcast(&reload_status, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (reload_status != 0) status = reload_status;

    free_global_solution(&counts, &displs, &x_global, &x_global_im);
    MPI_Finalize();
    return status ? EXIT_FAILURE : EXIT_SUCCESS;
}
