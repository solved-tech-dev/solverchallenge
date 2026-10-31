#include "solverchallenge26/petsc_binary_reader.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *sc26_status_string(int status)
{
    switch (status) {
    case SC26_SUCCESS: return "success";
    case SC26_ERR_ARGUMENT: return "invalid argument";
    case SC26_ERR_OPEN: return "cannot open file";
    case SC26_ERR_READ: return "short or failed file read";
    case SC26_ERR_FORMAT: return "invalid PETSc binary format";
    case SC26_ERR_SIZE: return "file size does not match header/scalar type";
    case SC26_ERR_MEMORY: return "temporary memory allocation failed";
    default: return "unknown error";
    }
}
static int sc26_host_is_little_endian(void)
{
    const uint16_t one = 1;
    return *((const unsigned char *)&one) == 1;
}

static uint32_t sc26_bswap32(uint32_t x)
{
    return ((x & UINT32_C(0x000000ff)) << 24) |
           ((x & UINT32_C(0x0000ff00)) << 8) |
           ((x & UINT32_C(0x00ff0000)) >> 8) |
           ((x & UINT32_C(0xff000000)) >> 24);
}

static uint64_t sc26_bswap64(uint64_t x)
{
    return ((x & UINT64_C(0x00000000000000ff)) << 56) |
           ((x & UINT64_C(0x000000000000ff00)) << 40) |
           ((x & UINT64_C(0x0000000000ff0000)) << 24) |
           ((x & UINT64_C(0x00000000ff000000)) << 8) |
           ((x & UINT64_C(0x000000ff00000000)) >> 8) |
           ((x & UINT64_C(0x0000ff0000000000)) >> 24) |
           ((x & UINT64_C(0x00ff000000000000)) >> 40) |
           ((x & UINT64_C(0xff00000000000000)) >> 56);
}

static uint32_t sc26_be32_to_host(uint32_t x)
{
    return sc26_host_is_little_endian() ? sc26_bswap32(x) : x;
}

static uint64_t sc26_be64_to_host(uint64_t x)
{
    return sc26_host_is_little_endian() ? sc26_bswap64(x) : x;
}

int sc26_read_be_i32(FILE *file, int32_t *value)
{
    uint32_t raw;
    if (fread(&raw, sizeof(raw), 1, file) != 1) return SC26_ERR_READ;
    raw = sc26_be32_to_host(raw);
    memcpy(value, &raw, sizeof(raw));
    return SC26_SUCCESS;
}

static int sc26_file_size(const char *path, uint64_t *bytes)
{
    struct stat st;
    if (!path || !bytes) return SC26_ERR_ARGUMENT;
    if (stat(path, &st) != 0) return SC26_ERR_OPEN;
    if (st.st_size < 0) return SC26_ERR_SIZE;
    *bytes = (uint64_t)st.st_size;
    return SC26_SUCCESS;
}

static int sc26_valid_scalar_type(sc26_scalar_type scalar_type)
{
    return scalar_type == SC26_SCALAR_REAL ||
           scalar_type == SC26_SCALAR_COMPLEX;
}

static int sc26_same_matrix_info(const sc26_matrix_info *a,
                                 const sc26_matrix_info *b)
{
    return a->rows == b->rows && a->cols == b->cols && a->nnz == b->nnz &&
           a->scalar_type == b->scalar_type &&
           a->scalar_components == b->scalar_components &&
           a->value_count == b->value_count && a->file_bytes == b->file_bytes;
}

static int sc26_same_vector_info(const sc26_vector_info *a,
                                 const sc26_vector_info *b)
{
    return a->length == b->length && a->scalar_type == b->scalar_type &&
           a->scalar_components == b->scalar_components &&
           a->value_count == b->value_count && a->file_bytes == b->file_bytes;
}

/* Read metadata only. The scalar type is supplied because PETSc does not
 * encode real/complex in the binary header. Exact file size is validated. */
int sc26_read_matrix_info(const char *path,
                                 sc26_scalar_type scalar_type,
                                 sc26_matrix_info *info)
{
    FILE *file;
    int32_t class_id, rows, cols, nnz;
    uint64_t file_bytes, expected_bytes, components;
    int status;

    if (!path || !info || !sc26_valid_scalar_type(scalar_type))
        return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_read_be_i32(file, &class_id);
    if (!status) status = sc26_read_be_i32(file, &rows);
    if (!status) status = sc26_read_be_i32(file, &cols);
    if (!status) status = sc26_read_be_i32(file, &nnz);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    if (status) return status;
    if (class_id != SC26_MAT_FILE_CLASSID || rows <= 0 || cols <= 0 || nnz < 0)
        return SC26_ERR_FORMAT;
    status = sc26_file_size(path, &file_bytes);
    if (status) return status;
    components = (uint64_t)(int)scalar_type;
    expected_bytes = UINT64_C(16) + UINT64_C(4) * (uint64_t)rows +
                     UINT64_C(4) * (uint64_t)nnz +
                     UINT64_C(8) * components * (uint64_t)nnz;
    if (file_bytes != expected_bytes) return SC26_ERR_SIZE;

    info->rows = rows;
    info->cols = cols;
    info->nnz = nnz;
    info->scalar_type = scalar_type;
    info->scalar_components = (int)scalar_type;
    info->value_count = components * (uint64_t)nnz;
    info->file_bytes = file_bytes;
    return SC26_SUCCESS;
}

int sc26_read_vector_info(const char *path,
                                 sc26_scalar_type scalar_type,
                                 sc26_vector_info *info)
{
    FILE *file;
    int32_t class_id, length;
    uint64_t file_bytes, expected_bytes, components;
    int status;

    if (!path || !info || !sc26_valid_scalar_type(scalar_type))
        return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_read_be_i32(file, &class_id);
    if (!status) status = sc26_read_be_i32(file, &length);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    if (status) return status;
    if (class_id != SC26_VEC_FILE_CLASSID || length <= 0)
        return SC26_ERR_FORMAT;
    status = sc26_file_size(path, &file_bytes);
    if (status) return status;
    components = (uint64_t)(int)scalar_type;
    expected_bytes = UINT64_C(8) +
                     UINT64_C(8) * components * (uint64_t)length;
    if (file_bytes != expected_bytes) return SC26_ERR_SIZE;

    info->length = length;
    info->scalar_type = scalar_type;
    info->scalar_components = (int)scalar_type;
    info->value_count = components * (uint64_t)length;
    info->file_bytes = file_bytes;
    return SC26_SUCCESS;
}

static int sc26_skip_matrix_header(FILE *file,
                                   const sc26_matrix_info *expected)
{
    int32_t class_id, rows, cols, nnz;
    int status = sc26_read_be_i32(file, &class_id);
    if (!status) status = sc26_read_be_i32(file, &rows);
    if (!status) status = sc26_read_be_i32(file, &cols);
    if (!status) status = sc26_read_be_i32(file, &nnz);
    if (status) return status;
    if (class_id != SC26_MAT_FILE_CLASSID || rows != expected->rows ||
        cols != expected->cols || nnz != expected->nnz)
        return SC26_ERR_FORMAT;
    return SC26_SUCCESS;
}

static int sc26_skip_vector_header(FILE *file,
                                   const sc26_vector_info *expected)
{
    int32_t class_id, length;
    int status = sc26_read_be_i32(file, &class_id);
    if (!status) status = sc26_read_be_i32(file, &length);
    if (status) return status;
    if (class_id != SC26_VEC_FILE_CLASSID || length != expected->length)
        return SC26_ERR_FORMAT;
    return SC26_SUCCESS;
}

static int sc26_read_csr_structure(FILE *file,
                                   const sc26_matrix_info *info,
                                   int32_t *row_ptr,
                                   int32_t *col_idx)
{
    uint64_t sum = 0;
    int32_t i;

    row_ptr[0] = 0;
    if (fread(row_ptr + 1, sizeof(int32_t), (size_t)info->rows, file) !=
        (size_t)info->rows)
        return SC26_ERR_READ;
    for (i = 0; i < info->rows; ++i) {
        uint32_t raw;
        int32_t row_length;
        memcpy(&raw, row_ptr + i + 1, sizeof(raw));
        raw = sc26_be32_to_host(raw);
        memcpy(&row_length, &raw, sizeof(raw));
        if (row_length < 0) return SC26_ERR_FORMAT;
        sum += (uint64_t)row_length;
        if (sum > (uint64_t)INT32_MAX) return SC26_ERR_FORMAT;
        row_ptr[i + 1] = (int32_t)sum;
    }
    if (sum != (uint64_t)info->nnz) return SC26_ERR_FORMAT;

    if (fread(col_idx, sizeof(int32_t), (size_t)info->nnz, file) !=
        (size_t)info->nnz)
        return SC26_ERR_READ;
    for (i = 0; i < info->nnz; ++i) {
        uint32_t raw;
        int32_t col;
        memcpy(&raw, col_idx + i, sizeof(raw));
        raw = sc26_be32_to_host(raw);
        memcpy(&col, &raw, sizeof(raw));
        if (col < 0 || col >= info->cols) return SC26_ERR_FORMAT;
        col_idx[i] = col;
    }
    return SC26_SUCCESS;
}

int sc26_read_values_f64(FILE *file, uint64_t count, double *values)
{
    uint64_t i;
    if (count > (uint64_t)SIZE_MAX) return SC26_ERR_SIZE;
    if (fread(values, sizeof(double), (size_t)count, file) != (size_t)count)
        return SC26_ERR_READ;
    for (i = 0; i < count; ++i) {
        uint64_t raw;
        memcpy(&raw, values + i, sizeof(raw));
        raw = sc26_be64_to_host(raw);
        memcpy(values + i, &raw, sizeof(raw));
    }
    return SC26_SUCCESS;
}

int sc26_read_values_f32(FILE *file, uint64_t count, float *values)
{
    const size_t chunk_capacity = (size_t)1 << 20;
    uint64_t *buffer = (uint64_t *)malloc(chunk_capacity * sizeof(*buffer));
    uint64_t done = 0;

    if (!buffer) return SC26_ERR_MEMORY;
    while (done < count) {
        uint64_t remain = count - done;
        size_t chunk = remain < (uint64_t)chunk_capacity ?
                       (size_t)remain : chunk_capacity;
        size_t i;
        if (fread(buffer, sizeof(*buffer), chunk, file) != chunk) {
            free(buffer);
            return SC26_ERR_READ;
        }
        for (i = 0; i < chunk; ++i) {
            uint64_t raw = sc26_be64_to_host(buffer[i]);
            double value;
            memcpy(&value, &raw, sizeof(value));
            values[(size_t)done + i] = (float)value;
        }
        done += (uint64_t)chunk;
    }
    free(buffer);
    return SC26_SUCCESS;
}

/*
 * Before a matrix load, allocate:
 *   row_ptr[info.rows + 1]
 *   col_idx[info.nnz]
 *   values[info.value_count]
 * Complex values use interleaved real/imag elements.
 */
int sc26_read_matrix_f64(const char *path,
                                const sc26_matrix_info *info,
                                int32_t *row_ptr,
                                int32_t *col_idx,
                                double *values)
{
    sc26_matrix_info actual;
    FILE *file;
    int status;
    if (!path || !info || !row_ptr || !col_idx || !values)
        return SC26_ERR_ARGUMENT;
    status = sc26_read_matrix_info(path, info->scalar_type, &actual);
    if (status) return status;
    if (!sc26_same_matrix_info(&actual, info)) return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_skip_matrix_header(file, info);
    if (!status) status = sc26_read_csr_structure(file, info, row_ptr, col_idx);
    if (!status) status = sc26_read_values_f64(file, info->value_count, values);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    return status;
}

int sc26_read_matrix_f32(const char *path,
                                const sc26_matrix_info *info,
                                int32_t *row_ptr,
                                int32_t *col_idx,
                                float *values)
{
    sc26_matrix_info actual;
    FILE *file;
    int status;
    if (!path || !info || !row_ptr || !col_idx || !values)
        return SC26_ERR_ARGUMENT;
    status = sc26_read_matrix_info(path, info->scalar_type, &actual);
    if (status) return status;
    if (!sc26_same_matrix_info(&actual, info)) return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_skip_matrix_header(file, info);
    if (!status) status = sc26_read_csr_structure(file, info, row_ptr, col_idx);
    if (!status) status = sc26_read_values_f32(file, info->value_count, values);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    return status;
}

/* Before a vector load, allocate values[info.value_count]. */
int sc26_read_vector_f64(const char *path,
                                const sc26_vector_info *info,
                                double *values)
{
    sc26_vector_info actual;
    FILE *file;
    int status;
    if (!path || !info || !values) return SC26_ERR_ARGUMENT;
    status = sc26_read_vector_info(path, info->scalar_type, &actual);
    if (status) return status;
    if (!sc26_same_vector_info(&actual, info)) return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_skip_vector_header(file, info);
    if (!status) status = sc26_read_values_f64(file, info->value_count, values);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    return status;
}

int sc26_read_vector_f32(const char *path,
                                const sc26_vector_info *info,
                                float *values)
{
    sc26_vector_info actual;
    FILE *file;
    int status;
    if (!path || !info || !values) return SC26_ERR_ARGUMENT;
    status = sc26_read_vector_info(path, info->scalar_type, &actual);
    if (status) return status;
    if (!sc26_same_vector_info(&actual, info)) return SC26_ERR_ARGUMENT;
    file = fopen(path, "rb");
    if (!file) return SC26_ERR_OPEN;
    status = sc26_skip_vector_header(file, info);
    if (!status) status = sc26_read_values_f32(file, info->value_count, values);
    if (fclose(file) != 0 && !status) status = SC26_ERR_READ;
    return status;
}
