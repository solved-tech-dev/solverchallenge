#define _POSIX_C_SOURCE 200809L

#include "product/xianzhi_solver.h"

#include "XianzhiKrylovAPI.h"

#include <dlfcn.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    SC26_XK_OK = 0,
    SC26_XK_ERR_ARGUMENT = -100,
    SC26_XK_ERR_MEMORY = -101,
    SC26_XK_ERR_IO = -102,
    SC26_XK_ERR_RUNTIME = -103,
    SC26_XK_ERR_PRODUCT = -104,
    SC26_XK_ERR_UNSUPPORTED = -105
};

struct sc26_hip_runtime {
    void *library;
    int ready;
    int (*malloc_fn)(void **, size_t);
    int (*free_fn)(void *);
    int (*memcpy_fn)(void *, const void *, size_t, int);
    int (*memset_fn)(void *, int, size_t);
    int (*set_device_fn)(int);
    int (*synchronize_fn)(void);
};

static struct sc26_hip_runtime hip_runtime;

struct sc26_xianzhi_solver {
    const struct SC26_SolverInput *input;
    xianzhikrylov_handle_t handle;
    xianzhikrylov_matrix_t matrix;
    xianzhikrylov_vector_t rhs;
    xianzhikrylov_vector_t solution;
    void *device_row_offsets;
    void *device_column_indices;
    void *device_values;
    void *device_rhs;
    void *device_solution;
    void *device_solution_low;
    int64_t n;
    int64_t nnz;
    int64_t rhs_count;
    int components;
    int dd_solution;
    int finished;
    char config_path[4096];
};

static uint64_t checked_bytes(uint64_t count, size_t width)
{
    if (width != 0 && count > UINT64_MAX / (uint64_t)width)
        return UINT64_MAX;
    return count * (uint64_t)width;
}

static int load_hip_runtime(void)
{
    const char *configured;
    if (hip_runtime.ready) return SC26_XK_OK;
    configured = getenv("SC26_HIP_RUNTIME_LIBRARY");
    if (configured && *configured)
        hip_runtime.library = dlopen(configured, RTLD_NOW | RTLD_LOCAL);
    if (!hip_runtime.library)
        hip_runtime.library = dlopen("libamdhip64.so", RTLD_NOW | RTLD_LOCAL);
    if (!hip_runtime.library)
        hip_runtime.library = dlopen("libgalaxyhip.so.5", RTLD_NOW | RTLD_LOCAL);
    if (!hip_runtime.library)
        hip_runtime.library = dlopen("libgalaxyhip.so", RTLD_NOW | RTLD_LOCAL);
    if (!hip_runtime.library) {
        fprintf(stderr, "cannot load HIP runtime: %s\n", dlerror());
        return SC26_XK_ERR_RUNTIME;
    }
#define SC26_LOAD_HIP(field, suffix)                                            \
    do {                                                                         \
        void *symbol = dlsym(hip_runtime.library, "hip" suffix);                 \
        if (!symbol) {                                                           \
            fprintf(stderr, "missing HIP symbol hip%s: %s\n", suffix, dlerror());\
            return SC26_XK_ERR_RUNTIME;                                          \
        }                                                                        \
        memcpy(&hip_runtime.field, &symbol, sizeof(symbol));                      \
    } while (0)
    SC26_LOAD_HIP(malloc_fn, "Malloc");
    SC26_LOAD_HIP(free_fn, "Free");
    SC26_LOAD_HIP(memcpy_fn, "Memcpy");
    SC26_LOAD_HIP(memset_fn, "Memset");
    SC26_LOAD_HIP(set_device_fn, "SetDevice");
    SC26_LOAD_HIP(synchronize_fn, "DeviceSynchronize");
#undef SC26_LOAD_HIP
    if (hip_runtime.set_device_fn(0) != 0) return SC26_XK_ERR_RUNTIME;
    hip_runtime.ready = 1;
    return SC26_XK_OK;
}

static int device_alloc(void **pointer, uint64_t bytes)
{
    void *value = NULL;
    if (!pointer || bytes > (uint64_t)SIZE_MAX) return SC26_XK_ERR_ARGUMENT;
    if (hip_runtime.malloc_fn(&value, (size_t)bytes) != 0 || !value)
        return SC26_XK_ERR_MEMORY;
    *pointer = value;
    return SC26_XK_OK;
}

static void device_free(void **pointer)
{
    if (pointer && *pointer) {
        (void)hip_runtime.free_fn(*pointer);
        *pointer = NULL;
    }
}

static int device_copy_to(void *destination, const void *source, uint64_t bytes)
{
    return hip_runtime.memcpy_fn(destination, source, (size_t)bytes, 1) == 0
               ? SC26_XK_OK
               : SC26_XK_ERR_RUNTIME;
}

static int device_copy_from(void *destination, const void *source, uint64_t bytes)
{
    return hip_runtime.memcpy_fn(destination, source, (size_t)bytes, 2) == 0
               ? SC26_XK_OK
               : SC26_XK_ERR_RUNTIME;
}

static int device_zero(void *destination, uint64_t bytes)
{
    return hip_runtime.memset_fn(destination, 0, (size_t)bytes) == 0
               ? SC26_XK_OK
               : SC26_XK_ERR_RUNTIME;
}

static char *read_text_file(const char *path)
{
    FILE *file;
    long length;
    char *text;
    if (!path) return NULL;
    file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    text = (char *)malloc((size_t)length + 1);
    if (!text) {
        fclose(file);
        return NULL;
    }
    if (fread(text, 1, (size_t)length, file) != (size_t)length) {
        free(text);
        fclose(file);
        return NULL;
    }
    text[length] = '\0';
    fclose(file);
    return text;
}

static int replace_all(char **text, const char *token, const char *value)
{
    size_t token_length;
    size_t value_length;
    size_t count = 0;
    size_t output_length;
    char *cursor;
    char *output;
    char *write;
    if (!text || !*text || !token || !*token) return SC26_XK_ERR_ARGUMENT;
    token_length = strlen(token);
    value_length = value ? strlen(value) : 0;
    for (cursor = *text; (cursor = strstr(cursor, token)) != NULL; cursor += token_length)
        ++count;
    if (count == 0) return SC26_XK_OK;
    if (!value) {
        fprintf(stderr, "configuration requires value for %s\n", token);
        return SC26_XK_ERR_ARGUMENT;
    }
    if (value_length < token_length) {
        if (count > SIZE_MAX / (token_length - value_length))
            return SC26_XK_ERR_MEMORY;
        output_length =
            strlen(*text) - count * (token_length - value_length);
    } else if (value_length > token_length) {
        if (count > SIZE_MAX / (value_length - token_length))
            return SC26_XK_ERR_MEMORY;
        output_length =
            strlen(*text) + count * (value_length - token_length);
    } else {
        output_length = strlen(*text);
    }
    output = (char *)malloc(output_length + 1);
    if (!output) return SC26_XK_ERR_MEMORY;
    cursor = *text;
    write = output;
    for (;;) {
        char *found = strstr(cursor, token);
        size_t prefix;
        if (!found) {
            strcpy(write, cursor);
            break;
        }
        prefix = (size_t)(found - cursor);
        memcpy(write, cursor, prefix);
        write += prefix;
        memcpy(write, value, value_length);
        write += value_length;
        cursor = found + token_length;
    }
    free(*text);
    *text = output;
    return SC26_XK_OK;
}

static int materialize_config(
    char *output,
    size_t output_size,
    const char *template_path,
    const char *provider_library,
    const char *provider_config,
    const char *pangulu_library)
{
    char *text;
    char temporary[] = "/tmp/sc26-xk-config-XXXXXX";
    int descriptor;
    FILE *file;
    size_t length;
    int status;
    if (!output || output_size == 0 || !template_path)
        return SC26_XK_ERR_ARGUMENT;
    text = read_text_file(template_path);
    if (!text) return SC26_XK_ERR_IO;
    status = replace_all(&text, "@LINGLONGAMG_LIBRARY@", provider_library);
    if (!status) status = replace_all(&text, "@LINGLONGAMG_CONFIG@", provider_config);
    if (!status) status = replace_all(&text, "@PANGULU_LIBRARY@", pangulu_library);
    if (status) {
        free(text);
        return status;
    }
    descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        free(text);
        return SC26_XK_ERR_IO;
    }
    file = fdopen(descriptor, "wb");
    if (!file) {
        close(descriptor);
        unlink(temporary);
        free(text);
        return SC26_XK_ERR_IO;
    }
    length = strlen(text);
    status = fwrite(text, 1, length, file) == length ? SC26_XK_OK : SC26_XK_ERR_IO;
    free(text);
    if (fclose(file) != 0 && !status) status = SC26_XK_ERR_IO;
    if (status || strlen(temporary) + 1 > output_size) {
        unlink(temporary);
        return status ? status : SC26_XK_ERR_ARGUMENT;
    }
    strcpy(output, temporary);
    return SC26_XK_OK;
}

static int upload_matrix_values(struct sc26_xianzhi_solver *solver)
{
    const size_t chunk_values = (size_t)1 << 20;
    double *staging;
    uint64_t offset;
    int status = SC26_XK_OK;
    if (solver->components == 1)
        return device_copy_to(
            solver->device_values, solver->input->val,
            checked_bytes((uint64_t)solver->nnz, sizeof(double)));
    staging = (double *)malloc(chunk_values * 2 * sizeof(double));
    if (!staging) return SC26_XK_ERR_MEMORY;
    for (offset = 0; offset < (uint64_t)solver->nnz; offset += chunk_values) {
        uint64_t count = (uint64_t)solver->nnz - offset;
        size_t index;
        if (count > chunk_values) count = chunk_values;
        for (index = 0; index < (size_t)count; ++index) {
            staging[2 * index] = solver->input->val[offset + index];
            staging[2 * index + 1] = solver->input->val_im[offset + index];
        }
        status = device_copy_to(
            (char *)solver->device_values + 2 * offset * sizeof(double),
            staging, count * 2 * sizeof(double));
        if (status) break;
    }
    free(staging);
    return status;
}

static int upload_rhs(struct sc26_xianzhi_solver *solver)
{
    const size_t chunk_values = (size_t)1 << 20;
    const double *rhs = solver->input->b_all ? solver->input->b_all : solver->input->b;
    const double *rhs_im = solver->input->b_im_all ? solver->input->b_im_all : solver->input->b_im;
    uint64_t scalar_count = (uint64_t)solver->n * (uint64_t)solver->rhs_count;
    double *staging = NULL;
    uint64_t offset;
    int status = SC26_XK_OK;
    if (!rhs) return SC26_XK_ERR_ARGUMENT;
    if (solver->components == 1)
        return device_copy_to(
            solver->device_rhs, rhs,
            checked_bytes(scalar_count, sizeof(double)));
    if (!rhs_im) return SC26_XK_ERR_ARGUMENT;
    staging = (double *)malloc(chunk_values * 2 * sizeof(double));
    if (!staging) return SC26_XK_ERR_MEMORY;
    for (offset = 0; offset < scalar_count; offset += chunk_values) {
        uint64_t count = scalar_count - offset;
        size_t index;
        if (count > chunk_values) count = chunk_values;
        for (index = 0; index < (size_t)count; ++index) {
            staging[2 * index] = rhs[offset + index];
            staging[2 * index + 1] = rhs_im[offset + index];
        }
        status = device_copy_to(
            (char *)solver->device_rhs + 2 * offset * sizeof(double),
            staging, count * 2 * sizeof(double));
        if (status) break;
    }
    free(staging);
    return status;
}

static int product_status(const char *operation, xianzhikrylov_status_t status)
{
    if (status == XIANZHIKRYLOV_SUCCESS || status == XIANZHIKRYLOV_CONVERGED)
        return SC26_XK_OK;
    fprintf(stderr, "%s failed with XianzhiKrylov status %d\n", operation, (int)status);
    return SC26_XK_ERR_PRODUCT;
}

int sc26_xianzhi_solver_create(
    struct sc26_xianzhi_solver **solver,
    const struct SC26_SolverInput *input,
    const struct sc26_xianzhi_options *options)
{
    struct sc26_xianzhi_solver *created;
    xianzhikrylov_matrix_data_t matrix_data;
    xianzhikrylov_value_type_t matrix_type;
    xianzhikrylov_value_type_t solution_type;
    uint64_t row_bytes;
    uint64_t column_bytes;
    uint64_t value_count;
    uint64_t vector_count;
    uint64_t vector_bytes;
    int status;
    if (!solver || !input || !options || !options->config_template)
        return SC26_XK_ERR_ARGUMENT;
    *solver = NULL;
    if (!input->row_ptr || !input->col_idx || !input->val || !input->x ||
        input->global_n <= 0 || input->global_n != input->local_n ||
        input->row_begin != 0 || input->rhs_count <= 0)
        return SC26_XK_ERR_UNSUPPORTED;
    if (options->complex_values) {
        if (!input->val_im || !input->b_im || !input->x_im)
            return SC26_XK_ERR_ARGUMENT;
    } else if (options->dd_solution && !input->x_all) {
        return SC26_XK_ERR_UNSUPPORTED;
    }
    if (load_hip_runtime()) return SC26_XK_ERR_RUNTIME;
    created = (struct sc26_xianzhi_solver *)calloc(1, sizeof(*created));
    if (!created) return SC26_XK_ERR_MEMORY;
    created->input = input;
    created->n = input->global_n;
    created->nnz = input->row_ptr[input->global_n];
    created->rhs_count = input->rhs_count;
    created->components = options->complex_values ? 2 : 1;
    created->dd_solution = options->complex_values ? 0 : options->dd_solution;
    status = materialize_config(
        created->config_path, sizeof(created->config_path),
        options->config_template, options->provider_library,
        options->provider_config, getenv("SC26_PANGULU_LIBRARY"));
    if (status) goto fail;
    row_bytes = checked_bytes((uint64_t)created->n + 1, sizeof(int));
    column_bytes = checked_bytes((uint64_t)created->nnz, sizeof(int));
    value_count = (uint64_t)created->nnz * (uint64_t)created->components;
    vector_count = (uint64_t)created->n * (uint64_t)created->rhs_count *
                   (uint64_t)created->components;
    vector_bytes = checked_bytes(vector_count, sizeof(double));
    if (row_bytes == UINT64_MAX || column_bytes == UINT64_MAX ||
        checked_bytes(value_count, sizeof(double)) == UINT64_MAX ||
        vector_bytes == UINT64_MAX) {
        status = SC26_XK_ERR_MEMORY;
        goto fail;
    }
    if ((status = device_alloc(&created->device_row_offsets, row_bytes)) ||
        (status = device_alloc(&created->device_column_indices, column_bytes)) ||
        (status = device_alloc(
             &created->device_values,
             checked_bytes(value_count, sizeof(double)))) ||
        (status = device_alloc(&created->device_rhs, vector_bytes)) ||
        (status = device_alloc(&created->device_solution, vector_bytes)))
        goto fail;
    if (created->dd_solution) {
        status = device_alloc(
            &created->device_solution_low,
            checked_bytes((uint64_t)created->n * (uint64_t)created->rhs_count,
                          sizeof(double)));
        if (status) goto fail;
    }
    if ((status = device_copy_to(
             created->device_row_offsets, input->row_ptr, row_bytes)) ||
        (status = device_copy_to(
             created->device_column_indices, input->col_idx, column_bytes)) ||
        (status = upload_matrix_values(created)) ||
        (status = upload_rhs(created)))
        goto fail;
    matrix_type = created->components == 2
                      ? XIANZHIKRYLOV_COMPLEX_FP64
                      : XIANZHIKRYLOV_FP64;
    solution_type = created->components == 2
                        ? XIANZHIKRYLOV_COMPLEX_FP64
                        : (created->dd_solution ? XIANZHIKRYLOV_DD
                                                : XIANZHIKRYLOV_FP64);
    memset(&matrix_data, 0, sizeof(matrix_data));
    matrix_data.abi_version = XIANZHIKRYLOV_ABI_VERSION;
    matrix_data.struct_size = sizeof(matrix_data);
    matrix_data.format = XIANZHIKRYLOV_CSR;
    matrix_data.value_type = matrix_type;
    matrix_data.index_type = XIANZHIKRYLOV_INDEX_I32;
    matrix_data.block_size = 1;
    matrix_data.block_layout = XIANZHIKRYLOV_BLOCK_ROW_MAJOR;
    matrix_data.memory_space = XIANZHIKRYLOV_DEVICE;
    matrix_data.device_id = 0;
    matrix_data.rows = created->n;
    matrix_data.columns = created->n;
    matrix_data.entries = created->nnz;
    matrix_data.row_offsets.data = created->device_row_offsets;
    matrix_data.row_offsets.bytes = row_bytes;
    matrix_data.column_indices.data = created->device_column_indices;
    matrix_data.column_indices.bytes = column_bytes;
    matrix_data.values.data = created->device_values;
    matrix_data.values.bytes = checked_bytes(value_count, sizeof(double));
    status = product_status(
        "xianzhikrylov_create", xianzhikrylov_create(0, &created->handle));
    if (!status)
        status = product_status(
            "xianzhikrylov_config_set",
            xianzhikrylov_config_set(
                created->handle, XIANZHIKRYLOV_CONFIG_FILE, created->config_path));
    if (!status)
        status = product_status(
            "xianzhikrylov_matrix_create",
            xianzhikrylov_matrix_create(&created->matrix, &matrix_data));
    if (!status)
        status = product_status(
            "xianzhikrylov_multiple_vector_create(rhs)",
            xianzhikrylov_multiple_vector_create(
                &created->rhs, created->n, created->rhs_count, created->n,
                XIANZHIKRYLOV_RHS_MAJOR, created->device_rhs, NULL,
                matrix_type, XIANZHIKRYLOV_DEVICE));
    if (!status)
        status = product_status(
            "xianzhikrylov_multiple_vector_create(solution)",
            xianzhikrylov_multiple_vector_create(
                &created->solution, created->n, created->rhs_count, created->n,
                XIANZHIKRYLOV_RHS_MAJOR, created->device_solution,
                created->device_solution_low, solution_type,
                XIANZHIKRYLOV_DEVICE));
    if (status) goto fail;
    if (hip_runtime.synchronize_fn() != 0) {
        status = SC26_XK_ERR_RUNTIME;
        goto fail;
    }
    *solver = created;
    return SC26_XK_OK;

fail:
    sc26_xianzhi_solver_destroy(created);
    return status;
}

int sc26_xianzhi_solver_analysis(struct sc26_xianzhi_solver *solver)
{
    if (!solver) return SC26_XK_ERR_ARGUMENT;
    return product_status(
        "xianzhikrylov_execute(ANALYSIS)",
        xianzhikrylov_execute(
            solver->handle, XIANZHIKRYLOV_PHASE_ANALYSIS,
            solver->matrix, NULL, NULL));
}

int sc26_xianzhi_solver_numeric_prepare(struct sc26_xianzhi_solver *solver)
{
    if (!solver) return SC26_XK_ERR_ARGUMENT;
    return product_status(
        "xianzhikrylov_execute(NUMERIC_PREPARE)",
        xianzhikrylov_execute(
            solver->handle, XIANZHIKRYLOV_PHASE_NUMERIC_PREPARE,
            solver->matrix, NULL, NULL));
}

int sc26_xianzhi_solver_solve(struct sc26_xianzhi_solver *solver)
{
    uint64_t scalar_count;
    uint64_t bytes;
    int status;
    if (!solver) return SC26_XK_ERR_ARGUMENT;
    scalar_count = (uint64_t)solver->n * (uint64_t)solver->rhs_count;
    bytes = checked_bytes(scalar_count, sizeof(double));
    if ((status = device_zero(solver->device_solution, bytes))) return status;
    if (solver->dd_solution &&
        (status = device_zero(solver->device_solution_low, bytes)))
        return status;
    status = product_status(
        "xianzhikrylov_execute(SOLVE)",
        xianzhikrylov_execute(
            solver->handle, XIANZHIKRYLOV_PHASE_SOLVE,
            solver->matrix, solver->solution, solver->rhs));
    if (!status && hip_runtime.synchronize_fn() != 0)
        status = SC26_XK_ERR_RUNTIME;
    return status;
}

static int copy_solution_to_host(struct sc26_xianzhi_solver *solver)
{
    const size_t chunk_values = (size_t)1 << 20;
    SC26_SolutionScalar *x =
        solver->input->x_all ? solver->input->x_all : solver->input->x;
    SC26_SolutionScalar *x_im =
        solver->input->x_im_all ? solver->input->x_im_all : solver->input->x_im;
    uint64_t scalar_count = (uint64_t)solver->n * (uint64_t)solver->rhs_count;
    uint64_t offset;
    double *high;
    double *low = NULL;
    int status = SC26_XK_OK;
    if (!x || (solver->components == 2 && !x_im)) return SC26_XK_ERR_ARGUMENT;
    high = (double *)malloc(chunk_values * sizeof(double));
    if (!high) return SC26_XK_ERR_MEMORY;
    if (solver->dd_solution) {
        low = (double *)malloc(chunk_values * sizeof(double));
        if (!low) {
            free(high);
            return SC26_XK_ERR_MEMORY;
        }
    }
    if (solver->components == 2) {
        double *interleaved = (double *)malloc(chunk_values * 2 * sizeof(double));
        if (!interleaved) {
            free(high);
            return SC26_XK_ERR_MEMORY;
        }
        for (offset = 0; offset < scalar_count; offset += chunk_values) {
            uint64_t count = scalar_count - offset;
            size_t index;
            if (count > chunk_values) count = chunk_values;
            status = device_copy_from(
                interleaved,
                (const char *)solver->device_solution +
                    2 * offset * sizeof(double),
                count * 2 * sizeof(double));
            if (status) break;
            for (index = 0; index < (size_t)count; ++index) {
                x[offset + index] = (SC26_SolutionScalar)interleaved[2 * index];
                x_im[offset + index] =
                    (SC26_SolutionScalar)interleaved[2 * index + 1];
            }
        }
        free(interleaved);
    } else {
        for (offset = 0; offset < scalar_count; offset += chunk_values) {
            uint64_t count = scalar_count - offset;
            size_t index;
            if (count > chunk_values) count = chunk_values;
            status = device_copy_from(
                high,
                (const char *)solver->device_solution + offset * sizeof(double),
                count * sizeof(double));
            if (!status && solver->dd_solution)
                status = device_copy_from(
                    low,
                    (const char *)solver->device_solution_low +
                        offset * sizeof(double),
                    count * sizeof(double));
            if (status) break;
            for (index = 0; index < (size_t)count; ++index) {
                x[offset + index] = (SC26_SolutionScalar)high[index];
                if (solver->dd_solution)
                    x[offset + index] += (SC26_SolutionScalar)low[index];
            }
        }
    }
    free(low);
    free(high);
    return status;
}

int sc26_xianzhi_solver_finish(struct sc26_xianzhi_solver *solver)
{
    int status;
    if (!solver || solver->finished) return SC26_XK_ERR_ARGUMENT;
    if (solver->handle)
        (void)xianzhikrylov_print_info(solver->handle);
    status = copy_solution_to_host(solver);
    solver->finished = 1;
    sc26_xianzhi_solver_destroy(solver);
    return status;
}

void sc26_xianzhi_solver_destroy(struct sc26_xianzhi_solver *solver)
{
    if (!solver) return;
    if (solver->rhs) (void)xianzhikrylov_vector_destroy(solver->rhs);
    if (solver->solution) (void)xianzhikrylov_vector_destroy(solver->solution);
    if (solver->matrix) (void)xianzhikrylov_matrix_destroy(solver->matrix);
    if (solver->handle) (void)xianzhikrylov_destroy(solver->handle);
    device_free(&solver->device_solution_low);
    device_free(&solver->device_solution);
    device_free(&solver->device_rhs);
    device_free(&solver->device_values);
    device_free(&solver->device_column_indices);
    device_free(&solver->device_row_offsets);
    if (solver->config_path[0]) unlink(solver->config_path);
    free(solver);
}
