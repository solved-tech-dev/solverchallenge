#ifndef SC26_PUBLIC_MPI_SYSTEM_LOADER_H
#define SC26_PUBLIC_MPI_SYSTEM_LOADER_H

/* MPI-local system loader public entry point. */
#include "system_loader.h"
#include "mpi.h"

struct MPI_LocalSystem {
    int global_rows;
    int global_cols;
    int global_nnz;
    int row_begin;
    int row_end;
    int local_nnz;
    int *row_ptr;
    int *col_idx;
    double *val;
    double *val_im;
    double *b;
    double *b_im;
};

#ifdef __cplusplus
extern "C" {
#endif

void mpi_local_system_init(struct MPI_LocalSystem *system);
void mpi_local_system_free(struct MPI_LocalSystem *system);
void mpi_row_partition(int rows, int rank, int size, int *begin, int *end);
void mpi_row_counts(int rows, int size, int *counts, int *displs);
int mpi_load_local_linear_system(const char *filename_matrix,
                                 const char *filename_b, int read_matrix_base,
                                 int sys_type, MPI_Comm comm,
                                 struct MPI_LocalSystem *system);

#ifdef __cplusplus
}
#endif

#endif
