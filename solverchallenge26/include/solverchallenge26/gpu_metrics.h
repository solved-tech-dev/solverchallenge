#ifndef GPU_METRICS_H
#define GPU_METRICS_H

#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>

/* CUDA event timer and memory helpers used by the GPU sample. */

#define SC26_CUDA_CHECK(call)                                                                                                               \
    do {                                                                                                                                   \
        cudaError_t sc26_cuda_status = (call);                                                                                             \
        if (sc26_cuda_status != cudaSuccess) {                                                                                             \
            fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(sc26_cuda_status));                      \
            exit(EXIT_FAILURE);                                                                                                           \
        }                                                                                                                                  \
    } while (0)

struct GpuTimer {
    cudaEvent_t start;
    cudaEvent_t stop;
};

static inline void gpu_timer_create(struct GpuTimer* timer)
{
    SC26_CUDA_CHECK(cudaEventCreate(&timer->start));
    SC26_CUDA_CHECK(cudaEventCreate(&timer->stop));
}

static inline void gpu_timer_start(struct GpuTimer* timer, cudaStream_t stream)
{
    SC26_CUDA_CHECK(cudaEventRecord(timer->start, stream));
}

static inline double gpu_timer_stop(struct GpuTimer* timer, cudaStream_t stream)
{
    float elapsed_ms = 0.0f;
    SC26_CUDA_CHECK(cudaEventRecord(timer->stop, stream));
    SC26_CUDA_CHECK(cudaEventSynchronize(timer->stop));
    SC26_CUDA_CHECK(cudaEventElapsedTime(&elapsed_ms, timer->start, timer->stop));
    return (double)elapsed_ms;
}

static inline void gpu_timer_destroy(struct GpuTimer* timer)
{
    SC26_CUDA_CHECK(cudaEventDestroy(timer->start));
    SC26_CUDA_CHECK(cudaEventDestroy(timer->stop));
}

struct GpuMemoryStats {
    size_t baseline_free;
    size_t minimum_free;
    size_t total;
    size_t tracked_peak;
};

/* Call after CUDA context/stream creation and before solver allocations. */
static inline void gpu_memory_begin(struct GpuMemoryStats* stats)
{
    SC26_CUDA_CHECK(cudaDeviceSynchronize());
    SC26_CUDA_CHECK(cudaMemGetInfo(&stats->baseline_free, &stats->total));
    stats->minimum_free = stats->baseline_free;
    stats->tracked_peak = 0;
}

/* Sample after stages that may allocate memory, especially factorization. */
static inline void gpu_memory_sample(struct GpuMemoryStats* stats, size_t tracked_bytes)
{
    size_t free_bytes;
    size_t total_bytes;
    SC26_CUDA_CHECK(cudaDeviceSynchronize());
    SC26_CUDA_CHECK(cudaMemGetInfo(&free_bytes, &total_bytes));
    if (free_bytes < stats->minimum_free) stats->minimum_free = free_bytes;
    if (tracked_bytes > stats->tracked_peak) stats->tracked_peak = tracked_bytes;
    stats->total = total_bytes;
}

static inline void gpu_mem_usage(const struct GpuMemoryStats* stats)
{
    size_t observed_peak = stats->baseline_free > stats->minimum_free ? stats->baseline_free - stats->minimum_free : 0;
    fprintf(stdout, "GPU memory usage:                   %12.4f MB\n", (double)stats->tracked_peak / (1024.0 * 1024.0));
    fprintf(stdout, "GPU observed memory delta:         %12.4f MB\n", (double)observed_peak / (1024.0 * 1024.0));
    fflush(stdout);
}

#endif
