#ifndef SC26_PUBLIC_SYSTEM_LOADER_H
#define SC26_PUBLIC_SYSTEM_LOADER_H

/* Serial system loader public entry point. */
#include "matrix_market_reader.h"
#include "petsc_binary_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

int file_is_petsc_matrix(const char *path);
int detect_sys_type_from_matrix(char *filename, int *sys_type);
int load_linear_system(char *filename_matrix, char *filename_b,
                       int read_matrix_base, int sys_type, int *m, int *n,
                       int *nnzA, int *isSymmetricA, int **row_ptr,
                       int **col_idx, double **val, double **val_im,
                       double **b, double **b_im);

#ifdef __cplusplus
}
#endif

#endif
