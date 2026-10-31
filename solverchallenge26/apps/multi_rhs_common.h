#ifndef SC26_MULTI_RHS_COMMON_H
#define SC26_MULTI_RHS_COMMON_H

#define _POSIX_C_SOURCE 200809L

#include "solverchallenge26/petsc_binary_reader.h"
#include "solverchallenge26/solver_input.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct sc26_multi_system {
    int n;
    int nnz;
    int is_complex;
    int rhs_count;
    int* row_ptr;
    int* col_idx;
    double* val;
    double* val_im;
    double* b_all;
    double* b_im_all;
    SC26_SolutionScalar* x_all;
    SC26_SolutionScalar* x_im_all;
};

static void sc26_multi_die(const char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

static void* sc26_multi_calloc(size_t count, size_t size)
{
    void* value;
    if (size != 0 && count > SIZE_MAX / size)
        sc26_multi_die("allocation size overflow");
    value = calloc(count ? count : 1, size ? size : 1);
    if (!value) sc26_multi_die("allocation failed");
    return value;
}

static void sc26_multi_free(struct sc26_multi_system* system)
{
    if (!system) return;
    free(system->x_im_all);
    free(system->x_all);
    free(system->b_im_all);
    free(system->b_all);
    free(system->val_im);
    free(system->val);
    free(system->col_idx);
    free(system->row_ptr);
    memset(system, 0, sizeof(*system));
}

static void sc26_multi_split_interleaved(
    const double* source, size_t count, double* real, double* imag)
{
    size_t index;
    for (index = 0; index < count; ++index) {
        real[index] = source[2 * index];
        imag[index] = source[2 * index + 1];
    }
}

static void sc26_multi_load(
    struct sc26_multi_system* system,
    const char* matrix_path,
    int is_complex,
    int rhs_count,
    char** rhs_paths)
{
    sc26_scalar_type scalar_type =
        is_complex ? SC26_SCALAR_COMPLEX : SC26_SCALAR_REAL;
    sc26_matrix_info matrix_info;
    sc26_vector_info vector_info;
    size_t scalar_count;
    double* raw;
    int status;
    int column;
    memset(system, 0, sizeof(*system));
    if (rhs_count <= 0) sc26_multi_die("at least one RHS is required");
    status = sc26_read_matrix_info(matrix_path, scalar_type, &matrix_info);
    if (status != SC26_SUCCESS)
        sc26_multi_die(sc26_status_string(status));
    if (matrix_info.rows != matrix_info.cols)
        sc26_multi_die("matrix must be square");
    system->n = matrix_info.rows;
    system->nnz = matrix_info.nnz;
    system->is_complex = is_complex;
    system->rhs_count = rhs_count;
    system->row_ptr =
        (int*)sc26_multi_calloc((size_t)system->n + 1, sizeof(int));
    system->col_idx =
        (int*)sc26_multi_calloc((size_t)system->nnz, sizeof(int));
    raw = (double*)sc26_multi_calloc(
        (size_t)matrix_info.value_count, sizeof(double));
    status = sc26_read_matrix_f64(
        matrix_path, &matrix_info, (int32_t*)system->row_ptr,
        (int32_t*)system->col_idx, raw);
    if (status != SC26_SUCCESS) {
        free(raw);
        sc26_multi_free(system);
        sc26_multi_die(sc26_status_string(status));
    }
    if (is_complex) {
        system->val =
            (double*)sc26_multi_calloc((size_t)system->nnz, sizeof(double));
        system->val_im =
            (double*)sc26_multi_calloc((size_t)system->nnz, sizeof(double));
        sc26_multi_split_interleaved(
            raw, (size_t)system->nnz, system->val, system->val_im);
        free(raw);
    } else {
        system->val = raw;
    }
    scalar_count = (size_t)system->n * (size_t)rhs_count;
    system->b_all =
        (double*)sc26_multi_calloc(scalar_count, sizeof(double));
    system->x_all = (SC26_SolutionScalar*)sc26_multi_calloc(
        scalar_count, sizeof(SC26_SolutionScalar));
    if (is_complex) {
        system->b_im_all =
            (double*)sc26_multi_calloc(scalar_count, sizeof(double));
        system->x_im_all = (SC26_SolutionScalar*)sc26_multi_calloc(
            scalar_count, sizeof(SC26_SolutionScalar));
    }
    for (column = 0; column < rhs_count; ++column) {
        status =
            sc26_read_vector_info(rhs_paths[column], scalar_type, &vector_info);
        if (status != SC26_SUCCESS)
            sc26_multi_die(sc26_status_string(status));
        if (vector_info.length != system->n)
            sc26_multi_die("RHS length does not match the matrix");
        raw = (double*)sc26_multi_calloc(
            (size_t)vector_info.value_count, sizeof(double));
        status = sc26_read_vector_f64(rhs_paths[column], &vector_info, raw);
        if (status != SC26_SUCCESS) {
            free(raw);
            sc26_multi_die(sc26_status_string(status));
        }
        if (is_complex) {
            sc26_multi_split_interleaved(
                raw, (size_t)system->n,
                system->b_all + (size_t)column * (size_t)system->n,
                system->b_im_all + (size_t)column * (size_t)system->n);
        } else {
            memcpy(
                system->b_all + (size_t)column * (size_t)system->n,
                raw, (size_t)system->n * sizeof(double));
        }
        free(raw);
    }
}

static void sc26_multi_input(
    struct SC26_SolverInput* input, struct sc26_multi_system* system)
{
    memset(input, 0, sizeof(*input));
    input->global_n = system->n;
    input->local_n = system->n;
    input->row_begin = 0;
    input->rhs_count = system->rhs_count;
    input->row_ptr = system->row_ptr;
    input->col_idx = system->col_idx;
    input->val = system->val;
    input->val_im = system->val_im;
    input->x = system->x_all;
    input->x_im = system->x_im_all;
    input->b = system->b_all;
    input->b_im = system->b_im_all;
    input->x_all = system->x_all;
    input->x_im_all = system->x_im_all;
    input->b_all = system->b_all;
    input->b_im_all = system->b_im_all;
}

static int sc26_multi_verify(
    const struct sc26_multi_system* system,
    const char* mode,
    double tolerance)
{
    int absolute_max =
        mode && strcmp(mode, "absolute_max") == 0;
    int column;
    int passed = 1;
    for (column = 0; column < system->rhs_count; ++column) {
        long double residual_sq = 0.0L;
        long double rhs_sq = 0.0L;
        long double max_residual = 0.0L;
        int row;
        for (row = 0; row < system->n; ++row) {
            long double ax_real = 0.0L;
            long double ax_imag = 0.0L;
            int entry;
            long double residual;
            for (entry = system->row_ptr[row];
                 entry < system->row_ptr[row + 1]; ++entry) {
                size_t index =
                    (size_t)column * (size_t)system->n +
                    (size_t)system->col_idx[entry];
                long double ar = system->val[entry];
                long double ai =
                    system->is_complex ? system->val_im[entry] : 0.0L;
                long double xr = system->x_all[index];
                long double xi =
                    system->is_complex ? system->x_im_all[index] : 0.0L;
                ax_real += ar * xr - ai * xi;
                ax_imag += ar * xi + ai * xr;
            }
            {
                size_t index = (size_t)column * (size_t)system->n + (size_t)row;
                long double br = system->b_all[index];
                long double bi =
                    system->is_complex ? system->b_im_all[index] : 0.0L;
                long double rr = br - ax_real;
                long double ri = bi - ax_imag;
                residual = sqrtl(rr * rr + ri * ri);
                residual_sq += residual * residual;
                rhs_sq += br * br + bi * bi;
                if (residual > max_residual) max_residual = residual;
            }
        }
        {
            long double rhs_norm = sqrtl(rhs_sq);
            long double relative =
                rhs_norm > 0.0L ? sqrtl(residual_sq) / rhs_norm
                                : sqrtl(residual_sq);
            int rhs_passed =
                absolute_max ? max_residual <= tolerance
                             : relative <= tolerance;
            printf(
                "RESIDUAL rhs=%d relative_l2=%.17Le absolute_max=%.17Le %s\n",
                column + 1, relative, max_residual,
                rhs_passed ? "PASS" : "FAIL");
            if (!rhs_passed) passed = 0;
        }
    }
    return passed;
}

static double sc26_multi_seconds(void)
{
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0)
        sc26_multi_die("clock_gettime failed");
    return (double)time.tv_sec + (double)time.tv_nsec * 1.0e-9;
}

#endif
