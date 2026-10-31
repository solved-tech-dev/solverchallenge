#include "solverchallenge26/mpi_system_loader.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void mpi_local_system_init(struct MPI_LocalSystem *system)
{
    memset(system, 0, sizeof(*system));
}

void mpi_local_system_free(struct MPI_LocalSystem *system)
{
    if (!system) return;
    free(system->row_ptr);
    free(system->col_idx);
    free(system->val);
    free(system->val_im);
    free(system->b);
    free(system->b_im);
    mpi_local_system_init(system);
}

void mpi_row_partition(int rows, int rank, int size, int *begin, int *end)
{
    int base = rows / size;
    int extra = rows % size;
    *begin = rank * base + (rank < extra ? rank : extra);
    *end = *begin + base + (rank < extra ? 1 : 0);
}

void mpi_row_counts(int rows, int size, int *counts, int *displs)
{
    int rank;
    for (rank = 0; rank < size; ++rank) {
        int begin, end;
        mpi_row_partition(rows, rank, size, &begin, &end);
        counts[rank] = end - begin;
        displs[rank] = begin;
    }
}

static int mpi_seek_u64(FILE *file, uint64_t offset)
{
    if (offset > (uint64_t)LLONG_MAX) return -1;
    return fseeko(file, (off_t)offset, SEEK_SET) == 0 ? 0 : -1;
}

static int mpi_read_local_columns(FILE *file, int *col_idx, int count)
{
    int i;
    for (i = 0; i < count; ++i) {
        int32_t value;
        if (sc26_read_be_i32(file, &value) != SC26_SUCCESS) return -1;
        col_idx[i] = (int)value;
    }
    return 0;
}

static int mpi_read_local_petsc(const char *filename_matrix, const char *filename_b,
                                int sys_type, MPI_Comm comm,
                                struct MPI_LocalSystem *system)
{
    int rank, size, local_status = 0, global_status;
    int row, i, local_rows;
    int nnz_before = 0, total_local_nnz = 0;
    sc26_scalar_type scalar_type = sys_type ? SC26_SCALAR_COMPLEX : SC26_SCALAR_REAL;
    sc26_matrix_info matrix_info;
    sc26_vector_info vector_info;
    FILE *file = NULL;
    int32_t *row_lengths = NULL;
    double *raw = NULL;

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    mpi_local_system_init(system);

    if (sc26_read_matrix_info(filename_matrix, scalar_type, &matrix_info) != SC26_SUCCESS)
        local_status = SC26_ERR_FORMAT;
    if (!local_status && sc26_read_vector_info(filename_b, scalar_type, &vector_info) != SC26_SUCCESS)
        local_status = SC26_ERR_FORMAT;
    if (!local_status && vector_info.length != matrix_info.rows)
        local_status = SC26_ERR_FORMAT;
    if (!local_status && matrix_info.rows != matrix_info.cols)
        local_status = SC26_ERR_FORMAT;
    MPI_Allreduce(&local_status, &global_status, 1, MPI_INT, MPI_MAX, comm);
    if (global_status) {
        if (rank == 0) fprintf(stdout, "MPI PETSc metadata read failed.\n");
        return -1;
    }

    mpi_row_partition((int)matrix_info.rows, rank, size,
                      &system->row_begin, &system->row_end);
    local_rows = system->row_end - system->row_begin;
    row_lengths = (int32_t *)malloc((size_t)local_rows * sizeof(*row_lengths));
    system->row_ptr = (int *)malloc((size_t)(local_rows + 1) * sizeof(*system->row_ptr));
    if ((local_rows && !row_lengths) || !system->row_ptr) local_status = SC26_ERR_MEMORY;

    if (!local_status) {
        file = fopen(filename_matrix, "rb");
        if (!file || mpi_seek_u64(file, UINT64_C(16) +
                                  UINT64_C(4) * (uint64_t)system->row_begin) != 0)
            local_status = SC26_ERR_OPEN;
    }
    if (!local_status) {
        for (i = 0; i < local_rows; ++i) {
            if (sc26_read_be_i32(file, &row_lengths[i]) != SC26_SUCCESS || row_lengths[i] < 0) {
                local_status = SC26_ERR_FORMAT;
                break;
            }
        }
    }
    if (file) {
        fclose(file);
        file = NULL;
    }
    MPI_Allreduce(&local_status, &global_status, 1, MPI_INT, MPI_MAX, comm);
    if (global_status) {
        free(row_lengths);
        mpi_local_system_free(system);
        if (rank == 0) fprintf(stdout, "MPI PETSc row metadata read failed.\n");
        return -1;
    }

    system->row_ptr[0] = 0;
    for (row = 0; row < local_rows; ++row)
        system->row_ptr[row + 1] = system->row_ptr[row] + (int)row_lengths[row];
    system->local_nnz = system->row_ptr[local_rows];
    free(row_lengths);
    row_lengths = NULL;

    MPI_Exscan(&system->local_nnz, &nnz_before, 1, MPI_INT, MPI_SUM, comm);
    if (rank == 0) nnz_before = 0;
    MPI_Allreduce(&system->local_nnz, &total_local_nnz, 1, MPI_INT, MPI_SUM, comm);
    if (nnz_before < 0 || total_local_nnz != matrix_info.nnz)
        local_status = SC26_ERR_FORMAT;

    system->col_idx = system->local_nnz ?
        (int *)malloc((size_t)system->local_nnz * sizeof(*system->col_idx)) : NULL;
    system->val = system->local_nnz ?
        (double *)malloc((size_t)system->local_nnz * sizeof(*system->val)) : NULL;
    if (system->local_nnz && (!system->col_idx || !system->val)) local_status = SC26_ERR_MEMORY;
    if (sys_type) {
        system->val_im = system->local_nnz ?
            (double *)malloc((size_t)system->local_nnz * sizeof(*system->val_im)) : NULL;
        if (system->local_nnz && !system->val_im) local_status = SC26_ERR_MEMORY;
    }
    system->b = local_rows ? (double *)malloc((size_t)local_rows * sizeof(*system->b)) : NULL;
    if (local_rows && !system->b) local_status = SC26_ERR_MEMORY;
    if (sys_type) {
        system->b_im = local_rows ?
            (double *)malloc((size_t)local_rows * sizeof(*system->b_im)) : NULL;
        if (local_rows && !system->b_im) local_status = SC26_ERR_MEMORY;
    }
    MPI_Allreduce(&local_status, &global_status, 1, MPI_INT, MPI_MAX, comm);
    if (global_status) {
        mpi_local_system_free(system);
        if (rank == 0) fprintf(stdout, "MPI PETSc local allocation failed.\n");
        return -1;
    }

    file = fopen(filename_matrix, "rb");
    if (!file || mpi_seek_u64(file, UINT64_C(16) +
                              UINT64_C(4) * (uint64_t)matrix_info.rows +
                              UINT64_C(4) * (uint64_t)nnz_before) != 0 ||
        mpi_read_local_columns(file, system->col_idx, system->local_nnz) != 0)
        local_status = SC26_ERR_READ;
    if (file) {
        fclose(file);
        file = NULL;
    }
    if (!local_status && system->local_nnz) {
        file = fopen(filename_matrix, "rb");
        if (!file || mpi_seek_u64(file, UINT64_C(16) +
                                  UINT64_C(4) * (uint64_t)matrix_info.rows +
                                  UINT64_C(4) * (uint64_t)matrix_info.nnz +
                                  UINT64_C(8) * (uint64_t)scalar_type * (uint64_t)nnz_before) != 0)
            local_status = SC26_ERR_READ;
        if (!local_status) {
            raw = (double *)malloc((size_t)system->local_nnz *
                                   (size_t)scalar_type * sizeof(*raw));
            if (!raw || sc26_read_values_f64(file,
                    (uint64_t)system->local_nnz * (uint64_t)scalar_type, raw) != SC26_SUCCESS)
                local_status = SC26_ERR_READ;
        }
        if (file) {
            fclose(file);
            file = NULL;
        }
        if (!local_status) {
            if (sys_type) {
                for (i = 0; i < system->local_nnz; ++i) {
                    system->val[i] = raw[2 * i];
                    system->val_im[i] = raw[2 * i + 1];
                }
            } else {
                memcpy(system->val, raw, (size_t)system->local_nnz * sizeof(*raw));
            }
        }
        free(raw);
        raw = NULL;
    }
    if (!local_status) {
        file = fopen(filename_b, "rb");
        if (!file || mpi_seek_u64(file, UINT64_C(8) +
                                  UINT64_C(8) * (uint64_t)scalar_type *
                                  (uint64_t)system->row_begin) != 0)
            local_status = SC26_ERR_READ;
        if (!local_status) {
            raw = (double *)malloc((size_t)local_rows * (size_t)scalar_type * sizeof(*raw));
            if (!raw || sc26_read_values_f64(file,
                    (uint64_t)local_rows * (uint64_t)scalar_type, raw) != SC26_SUCCESS)
                local_status = SC26_ERR_READ;
        }
        if (file) {
            fclose(file);
            file = NULL;
        }
        if (!local_status) {
            if (sys_type) {
                for (i = 0; i < local_rows; ++i) {
                    system->b[i] = raw[2 * i];
                    system->b_im[i] = raw[2 * i + 1];
                }
            } else if (local_rows) {
                memcpy(system->b, raw, (size_t)local_rows * sizeof(*raw));
            }
        }
        free(raw);
        raw = NULL;
    }
    MPI_Allreduce(&local_status, &global_status, 1, MPI_INT, MPI_MAX, comm);
    if (global_status) {
        mpi_local_system_free(system);
        if (rank == 0) fprintf(stdout, "MPI PETSc local data read failed.\n");
        return -1;
    }

    system->global_rows = matrix_info.rows;
    system->global_cols = matrix_info.cols;
    system->global_nnz = matrix_info.nnz;
    return 0;
}

/* Matrix Market remains supported through the existing parser, then is distributed once. */
static int mpi_read_mtx_and_scatter(const char *filename_matrix, const char *filename_b,
                                    int read_matrix_base, int sys_type, MPI_Comm comm,
                                    struct MPI_LocalSystem *system)
{
    int rank, size, status = 0, root_status = 0;
    int m = 0, n = 0, nnz = 0, symmetric = 0;
    int begin, end, local_rows, i;
    int *row_counts, *row_displs, *nnz_counts, *nnz_displs;
    int *full_row_ptr = NULL, *full_col_idx = NULL;
    double *full_val = NULL, *full_val_im = NULL, *full_b = NULL, *full_b_im = NULL;
    int *full_row_lengths = NULL;

    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);
    mpi_local_system_init(system);
    row_counts = (int *)malloc((size_t)size * sizeof(*row_counts));
    row_displs = (int *)malloc((size_t)size * sizeof(*row_displs));
    nnz_counts = (int *)malloc((size_t)size * sizeof(*nnz_counts));
    nnz_displs = (int *)malloc((size_t)size * sizeof(*nnz_displs));
    if (!row_counts || !row_displs || !nnz_counts || !nnz_displs) status = -1;
    int global_alloc_status = 0;
    MPI_Allreduce(&status, &global_alloc_status, 1, MPI_INT, MPI_MIN, comm);
    if (global_alloc_status) goto cleanup;
    if (rank == 0 && !status) {
        root_status = load_linear_system((char *)filename_matrix, (char *)filename_b,
                                         read_matrix_base, sys_type, &m, &n, &nnz, &symmetric,
                                         &full_row_ptr, &full_col_idx, &full_val, &full_val_im,
                                         &full_b, &full_b_im);
        if (root_status == 0 && m != n) root_status = -1;
    }
    MPI_Bcast(&root_status, 1, MPI_INT, 0, comm);
    if (root_status) {
        if (rank == 0) fprintf(stdout, "Matrix Market system read failed.\n");
        status = -1;
        goto cleanup;
    }
    MPI_Bcast(&m, 1, MPI_INT, 0, comm);
    MPI_Bcast(&n, 1, MPI_INT, 0, comm);
    MPI_Bcast(&nnz, 1, MPI_INT, 0, comm);
    mpi_row_counts(m, size, row_counts, row_displs);
    for (i = 0; i < size; ++i) {
        int b0, e0;
        mpi_row_partition(m, i, size, &b0, &e0);
        nnz_displs[i] = full_row_ptr ? full_row_ptr[b0] : 0;
        nnz_counts[i] = full_row_ptr ? full_row_ptr[e0] - full_row_ptr[b0] : 0;
    }
    MPI_Bcast(nnz_counts, size, MPI_INT, 0, comm);
    MPI_Bcast(nnz_displs, size, MPI_INT, 0, comm);
    mpi_row_partition(m, rank, size, &begin, &end);
    local_rows = end - begin;
    system->row_begin = begin;
    system->row_end = end;
    system->local_nnz = nnz_counts[rank];
    system->row_ptr = (int *)malloc((size_t)(local_rows + 1) * sizeof(*system->row_ptr));
    system->col_idx = system->local_nnz ? (int *)malloc((size_t)system->local_nnz * sizeof(*system->col_idx)) : NULL;
    system->val = system->local_nnz ? (double *)malloc((size_t)system->local_nnz * sizeof(*system->val)) : NULL;
    system->b = local_rows ? (double *)malloc((size_t)local_rows * sizeof(*system->b)) : NULL;
    if (sys_type) {
        system->val_im = system->local_nnz ? (double *)malloc((size_t)system->local_nnz * sizeof(*system->val_im)) : NULL;
        system->b_im = local_rows ? (double *)malloc((size_t)local_rows * sizeof(*system->b_im)) : NULL;
    }
    if (!system->row_ptr || (system->local_nnz && (!system->col_idx || !system->val)) ||
        (local_rows && !system->b) || (sys_type && ((system->local_nnz && !system->val_im) ||
                                                   (local_rows && !system->b_im)))) {
        status = -1;
        goto cleanup;
    }
    if (rank == 0) full_row_lengths = (int *)malloc((size_t)m * sizeof(*full_row_lengths));
    if (rank == 0 && !full_row_lengths) status = -1;
    int global_row_length_status = 0;
    MPI_Allreduce(&status, &global_row_length_status, 1, MPI_INT, MPI_MIN, comm);
    if (global_row_length_status) goto cleanup;
    if (rank == 0 && full_row_lengths)
        for (i = 0; i < m; ++i) full_row_lengths[i] = full_row_ptr[i + 1] - full_row_ptr[i];
    MPI_Scatterv(full_row_lengths, row_counts, row_displs, MPI_INT,
                 system->row_ptr + 1, row_counts[rank], MPI_INT, 0, comm);
    system->row_ptr[0] = 0;
    for (i = 0; i < local_rows; ++i) system->row_ptr[i + 1] += system->row_ptr[i];
    MPI_Scatterv(full_col_idx, nnz_counts, nnz_displs, MPI_INT,
                 system->col_idx, system->local_nnz, MPI_INT, 0, comm);
    MPI_Scatterv(full_val, nnz_counts, nnz_displs, MPI_DOUBLE,
                 system->val, system->local_nnz, MPI_DOUBLE, 0, comm);
    MPI_Scatterv(full_b, row_counts, row_displs, MPI_DOUBLE,
                 system->b, local_rows, MPI_DOUBLE, 0, comm);
    if (sys_type) {
        MPI_Scatterv(full_val_im, nnz_counts, nnz_displs, MPI_DOUBLE,
                     system->val_im, system->local_nnz, MPI_DOUBLE, 0, comm);
        MPI_Scatterv(full_b_im, row_counts, row_displs, MPI_DOUBLE,
                     system->b_im, local_rows, MPI_DOUBLE, 0, comm);
    }
    system->global_rows = m;
    system->global_cols = n;
    system->global_nnz = nnz;

cleanup:
    if (rank == 0) {
        free(full_row_ptr); free(full_col_idx); free(full_val); free(full_val_im);
        free(full_b); free(full_b_im);
    }
    free(full_row_lengths);
    free(row_counts); free(row_displs); free(nnz_counts); free(nnz_displs);
    if (status) mpi_local_system_free(system);
    return status;
}

int mpi_load_local_linear_system(const char *filename_matrix, const char *filename_b,
                                        int read_matrix_base, int sys_type, MPI_Comm comm,
                                        struct MPI_LocalSystem *system)
{
    int rank, is_petsc = 0;
    MPI_Comm_rank(comm, &rank);
    if (rank == 0) is_petsc = file_is_petsc_matrix(filename_matrix);
    MPI_Bcast(&is_petsc, 1, MPI_INT, 0, comm);
    if (is_petsc)
        return mpi_read_local_petsc(filename_matrix, filename_b, sys_type, comm, system);
    return mpi_read_mtx_and_scatter(filename_matrix, filename_b, read_matrix_base, sys_type, comm, system);
}
