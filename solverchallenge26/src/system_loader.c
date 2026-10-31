#include "solverchallenge26/system_loader.h"
#include "solverchallenge26/runtime_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int file_class_id(const char* path, int32_t* class_id)
{
    FILE* file;
    int   status;
    if (!path || !class_id) return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_read_be_i32(file, class_id);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    return status;
}
int file_is_petsc_matrix(const char* path)
{
    int32_t class_id;
    return file_class_id(path, &class_id) == SC26_SUCCESS && class_id == SC26_MAT_FILE_CLASSID;
}

static int file_is_petsc_vector(const char* path)
{
    int32_t class_id;
    return file_class_id(path, &class_id) == SC26_SUCCESS && class_id == SC26_VEC_FILE_CLASSID;
}

static void split_interleaved(const double* src, int count, double* re, double* im)
{
    int i;
    for (i = 0; i < count; i++) {
        re[i] = src[2 * (size_t)i];
        im[i] = src[2 * (size_t)i + 1];
    }
}

static int detect_petsc_sys_type(const char* path, int* sys_type)
{
    sc26_matrix_info info;
    if (sc26_read_matrix_info(path, SC26_SCALAR_REAL, &info) == SC26_SUCCESS) {
        *sys_type = 0;
        return 0;
    }
    if (sc26_read_matrix_info(path, SC26_SCALAR_COMPLEX, &info) == SC26_SUCCESS) {
        *sys_type = 1;
        return 0;
    }
    return -1;
}

/* Auto-detect real (0) vs complex (1). PETSc uses exact file size; MTX uses the banner. */
int detect_sys_type_from_matrix(char* filename, int* sys_type)
{
    if (file_is_petsc_matrix(filename)) return detect_petsc_sys_type(filename, sys_type);
    return read_mtx_header(filename, sys_type);
}

static int load_petsc_matrix(const char* path, int sys_type, int* m, int* n, int* nnz, int* isSymmetric, int** row_ptr, int** col_idx, double** val,
                             double** val_im)
{
    sc26_scalar_type st = sys_type ? SC26_SCALAR_COMPLEX : SC26_SCALAR_REAL;
    sc26_matrix_info info;
    if (sizeof(int) != sizeof(int32_t)) {
        fprintf(stdout, "PETSc loader requires 32-bit int.\n");
        return -1;
    }
    int32_t*         row32;
    int32_t*         col32;
    double*          values;
    int              status;

    status = sc26_read_matrix_info(path, st, &info);
    if (status) {
        fprintf(stdout, "PETSc matrix read failed: %s\n", sc26_status_string(status));
        return -1;
    }

    row32  = (int32_t*)malloc((size_t)(info.rows + 1) * sizeof(int32_t));
    col32  = (int32_t*)malloc((size_t)info.nnz * sizeof(int32_t));
    values = (double*)malloc((size_t)info.value_count * sizeof(double));
    if (!row32 || !col32 || !values) {
        free(row32);
        free(col32);
        free(values);
        fprintf(stdout, "PETSc matrix allocation failed.\n");
        return -1;
    }

    status = sc26_read_matrix_f64(path, &info, row32, col32, values);
    if (status) {
        free(row32);
        free(col32);
        free(values);
        fprintf(stdout, "PETSc matrix read failed: %s\n", sc26_status_string(status));
        return -1;
    }

    if (sys_type) {
        *val    = (double*)malloc((size_t)info.nnz * sizeof(double));
        *val_im = (double*)malloc((size_t)info.nnz * sizeof(double));
        if (!*val || !*val_im) {
            free(row32);
            free(col32);
            free(values);
            free(*val);
            free(*val_im);
            fprintf(stdout, "PETSc matrix allocation failed.\n");
            return -1;
        }
        split_interleaved(values, info.nnz, *val, *val_im);
        free(values);
    } else {
        *val    = values;
        *val_im = NULL;
    }

    *m            = info.rows;
    *n            = info.cols;
    *nnz          = info.nnz;
    *isSymmetric  = 0;
    *row_ptr      = (int*)row32;
    *col_idx      = (int*)col32;
    return 0;
}

static int load_petsc_vector(const char* path, int sys_type, int n, double** b, double** b_im)
{
    sc26_scalar_type st = sys_type ? SC26_SCALAR_COMPLEX : SC26_SCALAR_REAL;
    sc26_vector_info info;
    double*          values;
    int              status;

    status = sc26_read_vector_info(path, st, &info);
    if (status) {
        fprintf(stdout, "PETSc vector read failed: %s\n", sc26_status_string(status));
        return -1;
    }
    if (info.length != n) {
        fprintf(stdout, "Invalid size of vector.\n");
        return -1;
    }

    values = (double*)malloc((size_t)info.value_count * sizeof(double));
    if (!values) {
        fprintf(stdout, "PETSc vector allocation failed.\n");
        return -1;
    }

    status = sc26_read_vector_f64(path, &info, values);
    if (status) {
        free(values);
        fprintf(stdout, "PETSc vector read failed: %s\n", sc26_status_string(status));
        return -1;
    }

    if (sys_type) {
        *b    = (double*)malloc((size_t)n * sizeof(double));
        *b_im = (double*)malloc((size_t)n * sizeof(double));
        if (!*b || !*b_im) {
            free(values);
            free(*b);
            free(*b_im);
            fprintf(stdout, "PETSc vector allocation failed.\n");
            return -1;
        }
        split_interleaved(values, n, *b, *b_im);
        free(values);
    } else {
        *b    = values;
        *b_im = NULL;
    }
    return 0;
}

static int load_mtx_matrix_and_rhs(char* filename_matrix, char* filename_b, int read_matrix_base, int sys_type, int* m, int* n, int* nnzA,
                                   int* isSymmetricA, int** row_ptr, int** col_idx, double** val, double** val_im, double** b, double** b_im)
{
    if (sys_type == 0) {
        mmio_allinone(m, n, nnzA, isSymmetricA, &read_matrix_base, row_ptr, col_idx, val, filename_matrix);
        *val_im = NULL;
        *b      = (double*)malloc(sizeof(double) * (*n));
        *b_im   = NULL;
        if (!*b) {
            fprintf(stdout, "vector allocation failed.\n");
            return -1;
        }
        load_vector(*n, *b, filename_b);
    } else {
        mmio_allinone_complex(m, n, nnzA, isSymmetricA, &read_matrix_base, row_ptr, col_idx, val, val_im, filename_matrix);
        *b    = (double*)malloc(sizeof(double) * (*n));
        *b_im = (double*)malloc(sizeof(double) * (*n));
        if (!*b || !*b_im) {
            free(*b);
            free(*b_im);
            fprintf(stdout, "vector allocation failed.\n");
            return -1;
        }
        load_b_complex(*n, *b, *b_im, filename_b);
    }
    return 0;
}

/* Load A and b. PETSc binary is tried first; MTX is the fallback. */
int load_linear_system(char* filename_matrix, char* filename_b, int read_matrix_base, int sys_type, int* m, int* n, int* nnzA, int* isSymmetricA,
                              int** row_ptr, int** col_idx, double** val, double** val_im, double** b, double** b_im)
{
    if (file_is_petsc_matrix(filename_matrix)) {
        if (load_petsc_matrix(filename_matrix, sys_type, m, n, nnzA, isSymmetricA, row_ptr, col_idx, val, val_im) != 0) return -1;
        if (file_is_petsc_vector(filename_b)) {
            return load_petsc_vector(filename_b, sys_type, *n, b, b_im);
        }
        *b    = (double*)malloc(sizeof(double) * (*n));
        *b_im = sys_type ? (double*)malloc(sizeof(double) * (*n)) : NULL;
        if (!*b || (sys_type && !*b_im)) {
            free(*b);
            free(*b_im);
            fprintf(stdout, "vector allocation failed.\n");
            return -1;
        }
        if (sys_type)
            load_b_complex(*n, *b, *b_im, filename_b);
        else
            load_vector(*n, *b, filename_b);
        return 0;
    }

    return load_mtx_matrix_and_rhs(filename_matrix, filename_b, read_matrix_base, sys_type, m, n, nnzA, isSymmetricA, row_ptr, col_idx, val, val_im, b,
                                   b_im);
}
