#ifndef SC26_PUBLIC_PETSC_BINARY_READER_H
#define SC26_PUBLIC_PETSC_BINARY_READER_H

#include <stdint.h>
#include <stdio.h>

#define SC26_MAT_FILE_CLASSID 1211216
#define SC26_VEC_FILE_CLASSID 1211214

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SC26_SCALAR_REAL = 1,
    SC26_SCALAR_COMPLEX = 2
} sc26_scalar_type;

typedef enum {
    SC26_SUCCESS = 0,
    SC26_ERR_ARGUMENT = 1,
    SC26_ERR_OPEN = 2,
    SC26_ERR_READ = 3,
    SC26_ERR_FORMAT = 4,
    SC26_ERR_SIZE = 5,
    SC26_ERR_MEMORY = 6
} sc26_status;

typedef struct {
    int32_t rows;
    int32_t cols;
    int32_t nnz;
    sc26_scalar_type scalar_type;
    int scalar_components;
    uint64_t value_count;
    uint64_t file_bytes;
} sc26_matrix_info;

typedef struct {
    int32_t length;
    sc26_scalar_type scalar_type;
    int scalar_components;
    uint64_t value_count;
    uint64_t file_bytes;
} sc26_vector_info;

const char *sc26_status_string(int status);
int sc26_read_be_i32(FILE *file, int32_t *value);
int sc26_read_matrix_info(const char *path, sc26_scalar_type scalar_type,
                          sc26_matrix_info *info);
int sc26_read_vector_info(const char *path, sc26_scalar_type scalar_type,
                          sc26_vector_info *info);
int sc26_read_values_f64(FILE *file, uint64_t count, double *values);
int sc26_read_values_f32(FILE *file, uint64_t count, float *values);
int sc26_read_matrix_f64(const char *path, const sc26_matrix_info *info,
                         int32_t *row_ptr, int32_t *col_idx, double *values);
int sc26_read_matrix_f32(const char *path, const sc26_matrix_info *info,
                         int32_t *row_ptr, int32_t *col_idx, float *values);
int sc26_read_vector_f64(const char *path, const sc26_vector_info *info,
                         double *values);
int sc26_read_vector_f32(const char *path, const sc26_vector_info *info,
                         float *values);

#ifdef __cplusplus
}
#endif

#endif
