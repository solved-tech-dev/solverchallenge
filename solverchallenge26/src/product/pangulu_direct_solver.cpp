/*
 * PanguLU adapter for the direct-solve test programs.
 *
 * The adapter uploads the matrix and all RHS vectors for one round, owns a
 * private PanguLU instance, and copies that round's solution back before the
 * instance is destroyed.
 */
#include "product/pangulu_direct_solver.h"

#include <pangulu.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct HipRuntime {
    void* library                                          = nullptr;
    int   ready                                            = 0;
    int (*malloc_fn)(void**, std::size_t)                  = nullptr;
    int (*free_fn)(void*)                                  = nullptr;
    int (*memcpy_fn)(void*, const void*, std::size_t, int) = nullptr;
    int (*memset_fn)(void*, int, std::size_t)              = nullptr;
    int (*set_device_fn)(int)                              = nullptr;
    int (*synchronize_fn)()                                = nullptr;
    int (*mem_get_info_fn)(std::size_t*, std::size_t*)     = nullptr;
};

HipRuntime hip_runtime;

/* Resolve the few runtime functions needed for device buffers and copies. */
int load_hip_runtime()
{
    if (hip_runtime.ready) {
        return 0;
    }
    const char* configured = std::getenv("SC26_HIP_RUNTIME_LIBRARY");
    if (configured && *configured) {
        hip_runtime.library = dlopen(configured, RTLD_NOW | RTLD_LOCAL);
    }
    if (!hip_runtime.library) {
        hip_runtime.library = dlopen("libamdhip64.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!hip_runtime.library) {
        hip_runtime.library = dlopen("libgalaxyhip.so.5", RTLD_NOW | RTLD_LOCAL);
    }
    if (!hip_runtime.library) {
        hip_runtime.library = dlopen("libgalaxyhip.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!hip_runtime.library) {
        std::fprintf(stderr, "cannot load HIP runtime: %s\n", dlerror());
        return -1;
    }
#define SC26_LOAD_HIP(field, suffix)                                                                                                                 \
    do {                                                                                                                                             \
        void* symbol = dlsym(hip_runtime.library, "hip" suffix);                                                                                     \
        if (!symbol) {                                                                                                                               \
            std::fprintf(stderr, "missing HIP symbol hip%s: %s\n", suffix, dlerror());                                                               \
            return -1;                                                                                                                               \
        }                                                                                                                                            \
        std::memcpy(&hip_runtime.field, &symbol, sizeof(symbol));                                                                                    \
    } while (0)
    SC26_LOAD_HIP(malloc_fn, "Malloc");
    SC26_LOAD_HIP(free_fn, "Free");
    SC26_LOAD_HIP(memcpy_fn, "Memcpy");
    SC26_LOAD_HIP(memset_fn, "Memset");
    SC26_LOAD_HIP(set_device_fn, "SetDevice");
    SC26_LOAD_HIP(synchronize_fn, "DeviceSynchronize");
    SC26_LOAD_HIP(mem_get_info_fn, "MemGetInfo");
#undef SC26_LOAD_HIP
    if (hip_runtime.set_device_fn(0) != 0) {
        return -1;
    }
    hip_runtime.ready = 1;
    return 0;
}

/* Device allocation wrapper. */
int hip_alloc(void** pointer, std::size_t bytes) { return hip_runtime.malloc_fn(pointer, bytes); }

/* Device release wrapper. */
void hip_release(void* pointer)
{
    if (pointer) {
        (void)hip_runtime.free_fn(pointer);
    }
}

/* Host-to-device-copy wrapper. */
int hip_copy_to(void* destination, const void* source, std::size_t bytes) { return hip_runtime.memcpy_fn(destination, source, bytes, 1); }

/* Device-to-host-copy wrapper. */
int hip_copy_from(void* destination, const void* source, std::size_t bytes) { return hip_runtime.memcpy_fn(destination, source, bytes, 2); }

/* Clear a device buffer before a new solve. */
int hip_zero(void* destination, std::size_t bytes) { return hip_runtime.memset_fn(destination, 0, bytes); }

/* Read an optional string value from an environment variable or solver.json. */
std::string configured_value(const char* environment_name, const char* key)
{
    const char* environment = std::getenv(environment_name);
    if (environment && *environment) {
        return environment;
    }
#ifdef SC26_PROBLEM_CONFIG_DIR
    std::ifstream     input(std::string(SC26_PROBLEM_CONFIG_DIR) + "/solver.json");
    std::stringstream buffer;
    if (input) {
        buffer << input.rdbuf();
        const std::string text     = buffer.str();
        const std::string needle   = std::string("\"") + key + "\"";
        std::size_t       position = text.find(needle);
        if (position != std::string::npos) {
            position = text.find(':', position + needle.size());
            if (position != std::string::npos) {
                position = text.find('"', position + 1);
                if (position != std::string::npos) {
                    const std::size_t end = text.find('"', position + 1);
                    if (end != std::string::npos)
                        return text.substr(position + 1, end - position - 1);
                }
            }
        }
    }
#else
    (void)key;
#endif
    return {};
}
} // namespace

/* Complete state for one PanguLU direct-solve context. */
struct sc26_pangulu_direct_solver {
    const SC26_SolverInput* input           = nullptr;
    int64_t*                row_offsets     = nullptr;
    int32_t*                column_indices  = nullptr;
    double*                 matrix_values   = nullptr;
    double*                 rhs_values      = nullptr;
    double*                 solution_values = nullptr;
    pangulu_handle_t        handle          = nullptr;
    pangulu_matrix_t        matrix          = nullptr;
    pangulu_vector_t        rhs             = nullptr;
    pangulu_vector_t        solution        = nullptr;
    int64_t                 n               = 0;
    int64_t                 nnz             = 0;
    int64_t                 rhs_count       = 0;
    int                     components      = 1;
    std::size_t             baseline_free   = 0;
    std::size_t             minimum_free    = 0;
    std::size_t             device_total    = 0;

    ~sc26_pangulu_direct_solver()
    {
        if (handle) {
            pangulu_destroy(handle);
        }
        if (matrix) {
            pangulu_matrix_destroy(matrix);
        }
        if (rhs) {
            pangulu_vector_destroy(rhs);
        }
        if (solution) {
            pangulu_vector_destroy(solution);
        }
        hip_release(solution_values);
        hip_release(rhs_values);
        hip_release(matrix_values);
        hip_release(column_indices);
        hip_release(row_offsets);
    }
};

/* Report a product error and preserve a useful nonzero return code. */
static int fail(const char* operation, int status)
{
    std::fprintf(stderr, "%s failed: %d\n", operation, status);
    return status ? status : -1;
}

static void sample_device_memory(sc26_pangulu_direct_solver* solver)
{
    if (!solver || !solver->baseline_free) {
        return;
    }
    std::size_t free_bytes = 0;
    std::size_t total_bytes = 0;
    if (hip_runtime.mem_get_info_fn(&free_bytes, &total_bytes) == 0 &&
        free_bytes < solver->minimum_free) {
        solver->minimum_free = free_bytes;
    }
}

/*
 * Complex matrix values are interleaved before upload because that is the
 * layout expected by the PanguLU value type used here.
 */
static int upload_matrix_values(sc26_pangulu_direct_solver* solver)
{
    constexpr std::size_t kChunk         = std::size_t(1) << 20;
    const std::uint64_t   scalar_entries = std::uint64_t(solver->nnz) * std::uint64_t(solver->components);
    const std::uint64_t   bytes          = scalar_entries * sizeof(double);
    if (solver->components == 1) {
        const int status = hip_copy_to(solver->matrix_values, solver->input->val, static_cast<std::size_t>(bytes));
        return status == 0 ? 0 : fail("matrix upload", status);
    }
    std::vector<double> staging(kChunk * 2);
    for (std::uint64_t offset = 0; offset < std::uint64_t(solver->nnz); offset += kChunk) {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, std::uint64_t(solver->nnz) - offset));
        for (std::size_t index = 0; index < count; ++index) {
            staging[2 * index]     = solver->input->val[offset + index];
            staging[2 * index + 1] = solver->input->val_im[offset + index];
        }
        const int status = hip_copy_to(solver->matrix_values + 2 * offset, staging.data(), count * 2 * sizeof(double));
        if (status != 0) {
            return fail("complex matrix upload", status);
        }
    }
    return 0;
}

/* Upload all right-hand sides and interleave complex values when needed. */
static int upload_rhs(sc26_pangulu_direct_solver* solver)
{
    constexpr std::size_t kChunk       = std::size_t(1) << 20;
    const double*         rhs          = solver->input->b_all ? solver->input->b_all : solver->input->b;
    const double*         rhs_im       = solver->input->b_im_all ? solver->input->b_im_all : solver->input->b_im;
    const std::uint64_t   scalar_count = std::uint64_t(solver->n) * std::uint64_t(solver->rhs_count);
    if (solver->components == 1) {
        const int status = hip_copy_to(solver->rhs_values, rhs, static_cast<std::size_t>(scalar_count * sizeof(double)));
        return status == 0 ? 0 : fail("RHS upload", status);
    }
    std::vector<double> staging(kChunk * 2);
    for (std::uint64_t offset = 0; offset < scalar_count; offset += kChunk) {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, scalar_count - offset));
        for (std::size_t index = 0; index < count; ++index) {
            staging[2 * index]     = rhs[offset + index];
            staging[2 * index + 1] = rhs_im[offset + index];
        }
        const int status = hip_copy_to(solver->rhs_values + 2 * offset, staging.data(), count * 2 * sizeof(double));
        if (status != 0) {
            return fail("complex RHS upload", status);
        }
    }
    return 0;
}

/* Create the product objects and upload all input data before timing. */
extern "C" int sc26_pangulu_direct_solver_create(sc26_pangulu_direct_solver** solver, const SC26_SolverInput* input)
{
    if (!solver || !input || !input->row_ptr || !input->col_idx || !input->val || !input->b || !input->x || input->global_n <= 0 ||
        input->rhs_count <= 0) {
        return fail("PanguLU input validation", 1);
    }
    *solver = nullptr;
    std::unique_ptr<sc26_pangulu_direct_solver> owner(new (std::nothrow) sc26_pangulu_direct_solver);
    if (!owner) {
        return fail("PanguLU context allocation", 1);
    }
    auto* created       = owner.get();
    created->input      = input;
    created->n          = input->global_n;
    created->nnz        = input->row_ptr[input->global_n];
    created->rhs_count  = input->rhs_count;
    created->components = input->val_im ? 2 : 1;
    if (created->components == 2 && (!input->b_im || !input->x_im)) {
        return fail("PanguLU complex input validation", 1);
    }
    if (load_hip_runtime()) {
        return fail("hipSetDevice", 1);
    }
    if (hip_runtime.mem_get_info_fn(&created->baseline_free,
                                    &created->device_total) != 0) {
        created->baseline_free = 0;
        created->device_total  = 0;
    }
    created->minimum_free = created->baseline_free;
    const std::uint64_t value_count  = std::uint64_t(created->nnz) * std::uint64_t(created->components);
    const std::uint64_t vector_count = std::uint64_t(created->n) * std::uint64_t(created->rhs_count) * std::uint64_t(created->components);
    if (hip_alloc((void**)&created->row_offsets, (created->n + 1) * sizeof(*created->row_offsets)) != 0 ||
        hip_alloc((void**)&created->column_indices, created->nnz * sizeof(*created->column_indices)) != 0 ||
        hip_alloc((void**)&created->matrix_values, value_count * sizeof(*created->matrix_values)) != 0 ||
        hip_alloc((void**)&created->rhs_values, vector_count * sizeof(*created->rhs_values)) != 0 ||
        hip_alloc((void**)&created->solution_values, vector_count * sizeof(*created->solution_values)) != 0)
        return fail("PanguLU device allocation", 1);
    std::vector<int64_t> row_offsets(static_cast<std::size_t>(created->n + 1));
    for (int64_t row = 0; row <= created->n; ++row) {
        row_offsets[static_cast<std::size_t>(row)] = input->row_ptr[row];
    }
    if (hip_copy_to(created->row_offsets, row_offsets.data(), row_offsets.size() * sizeof(row_offsets[0])) != 0 ||
        hip_copy_to(created->column_indices, input->col_idx, static_cast<std::size_t>(created->nnz * sizeof(int32_t))) != 0)
        return fail("PanguLU matrix metadata upload", 1);
    int status = upload_matrix_values(created);
    if (!status) {
        status = upload_rhs(created);
    }
    if (status) {
        return status;
    }
    if (hip_zero(created->solution_values, static_cast<std::size_t>(vector_count * sizeof(*created->solution_values))) != 0)
        return fail("PanguLU solution initialization", 1);

    pangulu_config_value reordering            = L1;
    pangulu_config_value reuse                 = L2;
    const std::string    configured_reordering = configured_value("SC26_PANGULU_REORDERING", "reordering");
    const std::string    configured_reuse      = configured_value("SC26_PANGULU_REUSE", "reuse");
    if (configured_reordering == "L2") {
        reordering = L2;
    }
    if (configured_reuse == "L1") {
        reuse = L1;
    }
    const auto create_status = pangulu_create(0, &created->handle);
    if (create_status != PANGULU_STATUS_SUCCESS) {
        return fail("pangulu_create", static_cast<int>(create_status));
    }
    if (pangulu_config_set(created->handle, PANGULU_REORDERING_METHOD, reordering) != PANGULU_STATUS_SUCCESS ||
        pangulu_config_set(created->handle, PANGULU_REUSE_LEVEL, reuse) != PANGULU_STATUS_SUCCESS)
        return fail("pangulu_config_set", 1);
    const pangulu_value_type value_type = created->components == 2 ? PANGULU_C_64F : PANGULU_R_64F;
    if (pangulu_matrix_create(&created->matrix, PANGULU_MATRIX_LAYOUT_CSR, created->n, created->n, created->nnz, 1, created->row_offsets,
                              created->column_indices, created->matrix_values, PANGULU_R_64I, PANGULU_R_32I, value_type, PANGULU_LU,
                              PANGULU_MVIEW_FULL, PANGULU_MEMORY_DEVICE) != PANGULU_STATUS_SUCCESS)
        return fail("pangulu_matrix_create", 1);
    if (pangulu_multiple_vector_create(&created->rhs, created->n, created->rhs_count, created->n, created->rhs_values, value_type,
                                       PANGULU_MEMORY_DEVICE) != PANGULU_STATUS_SUCCESS ||
        pangulu_multiple_vector_create(&created->solution, created->n, created->rhs_count, created->n, created->solution_values, value_type,
                                       PANGULU_MEMORY_DEVICE) != PANGULU_STATUS_SUCCESS)
        return fail("pangulu_multiple_vector_create", 1);
    if (hip_runtime.synchronize_fn() != 0) {
        return fail("PanguLU upload synchronization", 1);
    }
    *solver = owner.release();
    return 0;
}

/* Run the structural analysis phase. */
extern "C" int sc26_pangulu_direct_solver_analysis(sc26_pangulu_direct_solver* solver)
{
    if (!solver) {
        return fail("PanguLU analysis", 1);
    }
    if (pangulu_execute(solver->handle, PANGULU_PHASE_ANALYSIS, solver->matrix, nullptr, nullptr) != PANGULU_STATUS_SUCCESS) {
        return fail("pangulu_execute(ANALYSIS)", 1);
    }
    sample_device_memory(solver);
    return 0;
}

/* Run the numeric factorization phase. */
extern "C" int sc26_pangulu_direct_solver_factorize(sc26_pangulu_direct_solver* solver)
{
    if (!solver) {
        return fail("PanguLU factorization", 1);
    }
    if (pangulu_execute(solver->handle, PANGULU_PHASE_NUMERIC_FACTORISATION, solver->matrix, nullptr, nullptr) != PANGULU_STATUS_SUCCESS) {
        return fail("pangulu_execute(NUMERIC_FACTORISATION)", 1);
    }
    sample_device_memory(solver);
    return 0;
}

/* Run the solve phase and wait for device completion before returning. */
extern "C" int sc26_pangulu_direct_solver_solve(sc26_pangulu_direct_solver* solver)
{
    if (!solver) {
        return fail("PanguLU solve", 1);
    }
    if (pangulu_execute(solver->handle, PANGULU_PHASE_SOLVE, solver->matrix, solver->solution, solver->rhs) != PANGULU_STATUS_SUCCESS)
        return fail("pangulu_execute(SOLVE)", 1);
    if (hip_runtime.synchronize_fn() != 0) {
        return fail("PanguLU solve synchronization", 1);
    }
    sample_device_memory(solver);
    return 0;
}

/* Copy the final device solution to the host after the measured rounds. */
static int copy_solution_to_host(sc26_pangulu_direct_solver* solver)
{
    constexpr std::size_t kChunk       = std::size_t(1) << 20;
    SC26_SolutionScalar*  x            = solver->input->x_all ? solver->input->x_all : solver->input->x;
    SC26_SolutionScalar*  x_im         = solver->input->x_im_all ? solver->input->x_im_all : solver->input->x_im;
    const std::uint64_t   scalar_count = std::uint64_t(solver->n) * std::uint64_t(solver->rhs_count);
    if (!x || (solver->components == 2 && !x_im)) {
        return fail("PanguLU solution destination", 1);
    }
    if (solver->components == 1) {
        std::vector<double> staging(kChunk);
        for (std::uint64_t offset = 0; offset < scalar_count; offset += kChunk) {
            const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, scalar_count - offset));
            if (hip_copy_from(staging.data(), solver->solution_values + offset, count * sizeof(double)) != 0)
                return fail("PanguLU solution copy", 1);
            for (std::size_t index = 0; index < count; ++index) {
                x[offset + index] = static_cast<SC26_SolutionScalar>(staging[index]);
            }
        }
        return 0;
    }
    std::vector<double> staging(kChunk * 2);
    for (std::uint64_t offset = 0; offset < scalar_count; offset += kChunk) {
        const std::size_t count = static_cast<std::size_t>(std::min<std::uint64_t>(kChunk, scalar_count - offset));
        if (hip_copy_from(staging.data(), solver->solution_values + 2 * offset, count * 2 * sizeof(double)) != 0)
            return fail("PanguLU complex solution copy", 1);
        for (std::size_t index = 0; index < count; ++index) {
            x[offset + index]    = static_cast<SC26_SolutionScalar>(staging[2 * index]);
            x_im[offset + index] = static_cast<SC26_SolutionScalar>(staging[2 * index + 1]);
        }
    }
    return 0;
}

/* Copy the final solution and release the product context. */
extern "C" int sc26_pangulu_direct_solver_finish(sc26_pangulu_direct_solver* solver)
{
    if (!solver) {
        return fail("PanguLU finish", 1);
    }
    sample_device_memory(solver);
    if (solver->baseline_free) {
        const std::size_t used_bytes = solver->baseline_free > solver->minimum_free
                                           ? solver->baseline_free - solver->minimum_free
                                           : 0;
        std::printf("GPU memory usage:                   %12.4f MB\n",
                    static_cast<double>(used_bytes) / (1024.0 * 1024.0));
        std::printf("GPU total memory:                   %12.4f MB\n",
                    static_cast<double>(solver->device_total) / (1024.0 * 1024.0));
    }
    const int status = copy_solution_to_host(solver);
    sc26_pangulu_direct_solver_destroy(solver);
    return status;
}

/* Release all resources owned by the adapter context. */
extern "C" void sc26_pangulu_direct_solver_destroy(sc26_pangulu_direct_solver* solver) { delete solver; }
