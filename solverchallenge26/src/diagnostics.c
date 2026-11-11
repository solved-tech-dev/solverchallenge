#include "solverchallenge26/residual_check.h"
#include "solverchallenge26/matrix_market_reader.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#ifdef MPI_USE
#include "mpi.h"
#endif

// Thanks to Yi Zong for providing support!
// check memory using function
void mem_usage();

// user define by shb & milkyway
// #define MM_MAX_LINE_LENGTH  __INT32_MAX__
#include <math.h>
#include <sys/resource.h>
#include <sys/time.h>
#ifdef MPI_USE
#include "mpi.h"
#endif

/**
 * \brief Definition of max, min, abs
 */
#ifndef _MAX_MIN_ABS_
#define _MAX_MIN_ABS_
#define MAX(a, b) (((a) > (b)) ? (a) : (b)) ///< bigger one in a and b
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) ///< smaller one in a and b
#define ABS(a) (((a) >= 0.0) ? (a) : -(a))  ///< absolute value of a
#endif

// check answer function
void check_correctness(int n, int* row_ptr, int* col_idx, double* val, double* x, double* b);

void check_correctness_complex(int n, int* row_ptr, int* col_idx, double* val, double* val_v, double* x, double* xi, double* b, double* bi);

void check_correctness_ld_xld(int n, int* row_ptr, int* col_idx, double* val, long double* x, double* b);
void check_correctness_complex_ld_xld(int n, int* row_ptr, int* col_idx, double* val, double* vali, long double* x, long double* xi, double* b, double* bi);

// store vector function
void store_x(int n, double* x, char* filename);

// read vector function
void load_vector(int n, double* b, char* filename);

void mem_usage()
{
    struct rusage r_usage;
    getrusage(RUSAGE_SELF, &r_usage);
    long long int mymem = r_usage.ru_maxrss;
#ifdef MPI_USE
    long long int total;
    MPI_Allreduce(&mymem, &total, 1, MPI_LONG_LONG_INT, MPI_SUM, MPI_COMM_WORLD);
    int myrank;
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    if (myrank == 0) {
        fprintf(stdout, "Total memory usage:                 %.4f MB\n", (double)((double)(total) / (double)(1024)));
        fflush(stdout);
    }
#else
    fprintf(stdout, "Total memory usage:                 %.4f MB\n", (double)((double)(mymem) / (double)(1024)));
    fflush(stdout);
#endif
}

void print_help()
{
    fprintf(stdout, "please follow this input:\n./solver data/matrix.dat data/vector.dat [read_base] [sys_type]\n");
    fprintf(stdout, "e.g., ./solver data/matrix.dat data/vector.dat 0 0\n");
    fprintf(stdout, "\nhelp:\n");
    fprintf(stdout, "matrix.dat:     PETSc binary matrix A (default); Matrix Market .mtx is also accepted\n");
    fprintf(stdout, "vector.dat:     PETSc binary right-hand side b; .rhs is used with MTX matrices\n");
    fprintf(stdout, "read_base:      MTX only, 0-base or 1-base, default 1-base; PETSc is always 0-base\n");
    fprintf(stdout, "timing:         mean of rounds 6-10; every round uses a fresh solve context\n");
    fprintf(stdout, "sys_type:       type of algebraic systems, 0: real, 1: complex; default auto-detect\n");
}

// Multiply a csr matrix with a vector x, and get the resulting vector y ,sum use kekan sum
void spmv(int n, int* row_ptr, int* col_idx, double* val, double* x, double* y)
{
    for (int i = 0; i < n; i++) {
        y[i]     = 0.0;
        double c = 0.0;
        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            double num = val[j] * x[col_idx[j]];
            double z   = num - c;
            double t   = y[i] + z;
            c          = (t - y[i]) - z;
            y[i]       = t;
        }
    }
}

// The complex number multiplication
void mul(double* v1, double* v1i, double* v2, double* v2i, double* v3, double* v3i)
{
    *v3  = *v1 * *v2 - (*v1i * *v2i);
    *v3i = *v1i * *v2 + *v1 * *v2i;
}

// The complex number multiplication
void conj_mul(double* v1, double* v1i, double* v2, double* v2i, double* v3, double* v3i)
{
    *v3  = *v1 * *v2 + (*v1i * *v2i);
    *v3i = *v1 * *v2i - *v1i * *v2;
}

// The complex number addition ,sum use kekan sum
void add(double* v1, double* v1i, double* sum, double* sumi, double* c, double* ci)
{
    double num = *v1;
    double z   = num - *c;
    double t   = *sum + z;
    *c         = (t - *sum) - z;
    *sum       = t;

    double numi = *v1i;
    double zi   = numi - *ci;
    double ti   = *sumi + zi;
    *ci         = (ti - *sumi) - zi;
    *sumi       = ti;
}

// Multiply a csr matrix with two vector x and xi, and get the resulting vector y and yi
void spmv_complex(int n, int* row_ptr, int* col_idx, double* val, double* vali, double* x, double* xi, double* y, double* yi)
{
    double tmp[2];
    double c[2];
    for (int i = 0; i < n; i++) {
        y[i] = yi[i] = 0.0;
        c[0] = c[1] = 0.0;
        // printf(c + 1);

        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            mul(val + j, vali + j, x + col_idx[j], xi + col_idx[j], tmp, tmp + 1);
            add(tmp, tmp + 1, y + i, yi + i, c, c + 1);
        }
    }
}

// Calculate the mode of complex number
void complex_modulus_squared(double* a, double* ai, double* l) { *l = *a * *a + (*ai * *ai); }

// Calculate the conjugate of complex number
void complex_conjugate(double* a, double* ai, double* b, double* bi)
{
    *b  = *a;
    *bi = -*ai;
}

// Calculate the division of complex number
void complex_division(double* a, double* ai, double* b, double* bi, double* c, double* ci)
{
    double  tmp = 0.0;
    double* l;
    l = &tmp;
    complex_modulus_squared(b, bi, l);
    *c  = *a * *b + (*ai * *bi);
    *ci = *ai * *b - (*a * *bi);
    *c  = (*c) / (*l);
    *ci = (*ci) / (*l);
}

// Calculate the 2-norm of a vector ,sum use kekan sum
double vec2norm(double* x, int n)
{
    double sum = 0.0;
    double c   = 0.0;
    for (int i = 0; i < n; i++) {
        double num = x[i] * x[i];
        double z   = num - c;
        double t   = sum + z;
        c          = (t - sum) - z;
        sum        = t;
    }

    return sqrt(sum);
}

// Calculate the 2-norm of a complex vector ,sum use kekan sum
void vec2norm_complex(double* x, double* xi, double* err, double* erri, int n)
{
    double sum[2] = {0, 0}, tmp[2];
    double c[2];
    c[0] = c[1] = 0.0;

    for (int i = 0; i < n; i++) {
        conj_mul(x + i, xi + i, x + i, xi + i, tmp, tmp + 1);
        add(tmp, tmp + 1, sum, sum + 1, c, c + 1);
    }

    *err  = sqrt(sum[0]);
    *erri = sqrt(sum[1]);
}

#include <float.h>
double max_check(double* x, int n)
{
    double max = DBL_MIN;
    for (int i = 0; i < n; i++) {
        double x_fabs = fabs(x[i]);
        max           = max > x_fabs ? max : x_fabs;
    }
    return max;
}

double max_check_complex(double* x, double* xi, int n)
{
    double max = DBL_MIN;
    for (int i = 0; i < n; i++) {
        double x_fabs = x[i] * x[i] + xi[i] * xi[i];
        max           = max > x_fabs ? max : x_fabs;
    }
    return sqrt(max);
}

// validate the x
// answer1 = sqrt(|| A*x - b ||)
// answer2 = || A*x - b || MAX
// answer3 = sqrt(|| A*x - b ||/|| b ||)
// precision check using double type
// void check_correctness(int n, int *row_ptr, int *col_idx, double *val, double *x, double *b)
void check_correctness(int n, int* row_ptr, int* col_idx, double* val, double* x, double* b)
{

    double* b_new   = (double*)malloc(sizeof(double) * n);
    double* check_b = (double*)malloc(sizeof(double) * n);
    // double* r_b     = (double*)malloc(sizeof(double) * n);

    spmv(n, row_ptr, col_idx, val, x, b_new);
    for (int i = 0; i < n; i++) {
        check_b[i] = b_new[i] - b[i];
        // r_b[i]     = fabs(check_b[i]) / MAX((b[i]), 1e-20);
    }

    double answer1 = vec2norm(check_b, n);
    double answer2 = max_check(check_b, n);
    double answer3 = answer1 / vec2norm(b, n);
    // double answer4 = max_check(r_b, n);
    fprintf(stdout, "Check || b - Ax || 2             =  %12.6e\n", answer1);
    fprintf(stdout, "Check || b - Ax || MAX           =  %12.6e\n", answer2);
    fprintf(stdout, "Check || b - Ax || 2 / || b || 2 =  %12.6e\n", answer3);
    // fprintf(stdout, "Check MAX { |b - Ax|_i / |b_i| } =  %12.6e\n", answer4);

    free(b_new);
    free(check_b);
    // free(r_b);
}

void check_correctness_complex(int n, int* row_ptr, int* col_idx, double* val, double* val_v, double* x, double* xi, double* b, double* bi)
{
    double* b_new     = (double*)malloc(sizeof(double) * n);
    double* b_new_i   = (double*)malloc(sizeof(double) * n);
    double* check_b   = (double*)malloc(sizeof(double) * n);
    double* check_b_i = (double*)malloc(sizeof(double) * n);

    double* r_b     = (double*)malloc(sizeof(double) * n);
    double* r_b_i   = (double*)malloc(sizeof(double) * n);
    // double* rb_mode = (double*)malloc(sizeof(double) * n);
    // double  tmp     = 0.0;
    // double* l;
    // l = &tmp;

    spmv_complex(n, row_ptr, col_idx, val, val_v, x, xi, b_new, b_new_i);
    for (int i = 0; i < n; i++) {
        check_b[i]   = b_new[i] - b[i];
        check_b_i[i] = b_new_i[i] - bi[i];
        // complex_division(&check_b[i], &check_b_i[i], &b[i], &bi[i], r_b, r_b_i);
        // complex_modulus_squared(r_b, r_b_i, l);
        // rb_mode[i] = sqrtl(*l);
    }

    double err_check_b  = 0.0;
    double err_check_bi = 0.0;
    double err_b        = 0.0;
    double err_bi       = 0.0;

    vec2norm_complex(check_b, check_b_i, &err_check_b, &err_check_bi, n);
    vec2norm_complex(b, bi, &err_b, &err_bi, n);
    double max_answer  = max_check_complex(check_b, check_b_i, n);
    // double max_answer2 = max_check(rb_mode, n);

    fprintf(stdout, "Check complex || b - Ax || 2             =  %12.6e \n", err_check_b);
    fprintf(stdout, "Check complex || b - Ax || MAX           =  %12.6e \n", max_answer);
    fprintf(stdout, "Check complex || b - Ax || 2 / || b || 2 =  %12.6e \n", err_check_b / err_b);
    // fprintf(stdout, "Check complex MAX { |b - Ax|_i / |b_i| } =  %12.6e \n", max_answer2);

    free(b_new);
    free(b_new_i);
    free(check_b);
    free(check_b_i);
    free(r_b);
    free(r_b_i);
    // free(rb_mode);
}

// store x to a file
void store_x(int n, double* x, char* filename)
{
    FILE* p = fopen(filename, "w");
    if (!p) { perror(filename); exit(EXIT_FAILURE); }
    fprintf(p, "%d\n", n);
    for (int i = 0; i < n; i++) fprintf(p, "%lf\n", x[i]);
    fclose(p);
}

// store x (complex type) to a file
void store_x_complex(int n, double* x, double* x_v, char* filename)
{
    FILE* p = fopen(filename, "w");
    if (!p) { perror(filename); exit(EXIT_FAILURE); }
    fprintf(p, "%d\n", n);
    int i;
    for (i = 0; i < n; i++) fprintf(p, "%lf %lf\n", x[i], x_v[i]);
    fclose(p);
}

// store x (complex type) to a file, long double version
void store_x_complex_ld(int n, long double* x, long double* x_v, char* filename)
{
    FILE* p = fopen(filename, "w");
    if (!p) { perror(filename); exit(EXIT_FAILURE); }
    fprintf(p, "%d\n", n);
    int i;
    for (i = 0; i < n; i++) fprintf(p, "%.18Le %.18Le\n", x[i], x_v[i]);
    fclose(p);
}

// load right-hand side vector b
void load_vector(int n, double* b, char* filename)
{
    FILE* p = fopen(filename, "r");
    int   n_right;
    // MM_typecode matcode;

    int  r = 0;
    char line[MM_MAX_LINE_LENGTH];
    fgets(line, MM_MAX_LINE_LENGTH, p);

    if (line[0] == '%') {
        r = fscanf(p, "%d", &n_right);
    } else {
        n_right = atoi(line);
    }

    if (n_right != n) {
        fclose(p);
        fprintf(stdout, "Invalid size of vector.\n");
        return;
    }
    for (int i = 0; i < n_right; i++) r = fscanf(p, "%lf", &b[i]);
    fclose(p);
}

// load right-hand side vector b (complex type)
void load_b_complex(int n, double* b, double* b_v, char* filename)
{
    FILE* p = fopen(filename, "r");
    int   n_right;
    // MM_typecode matcode;

    int  r = 0;
    char line[MM_MAX_LINE_LENGTH];
    fgets(line, MM_MAX_LINE_LENGTH, p);

    if (line[0] == '%') {
        r = fscanf(p, "%d", &n_right);
    } else {
        n_right = atoi(line);
    }

    if (n_right != n) {
        fclose(p);
        fprintf(stdout, "Invalid size of vector.\n");
        return;
    }
    for (int i = 0; i < n_right; i++) r = fscanf(p, "%lf %lf", &b[i], &b_v[i]);
    fclose(p);
}

// return now time
double GetCurrentTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec * 1000.0 + time.tv_usec / 1000.0;
}



// Thanks to Haibing Sun and Li Zhao for providing support for long double precision
// detection. Here, long double is abbreviated as "ld"

#include <float.h>
#include <math.h>
#include <sys/time.h>

/**
 * \brief Definition of max, min, abs
 */
#ifndef _MAX_MIN_ABS_
#define _MAX_MIN_ABS_
#define MAX(a, b) (((a) > (b)) ? (a) : (b)) ///< bigger one in a and b
#define MIN(a, b) (((a) < (b)) ? (a) : (b)) ///< smaller one in a and b
#define ABS(a) (((a) >= 0.0) ? (a) : -(a))  ///< absolute value of a
#endif

// check answer function
void check_correctness_ld(int n, int* row_ptr, int* col_idx, long double* val, long double* x, long double* b);

void check_correctness_ld_d2ld(int n, int* row_ptr, int* col_idx, double* val, double* x, double* b);

void check_correctness_complex_ld(int n, int* row_ptr, int* col_idx, long double* val, long double* vali, long double* x, long double* xi,
                                  long double* b, long double* bi);

void check_correctness_complex_ld_d2ld(int n, int* row_ptr, int* col_idx, double* val, double* vali, double* x, double* xi, double* b, double* bi);

// store vector function
void store_x_ld(int n, long double* x, char* filename);

void store_x_complex_ld(int n, long double* x, long double* x_v, char* filename);

// read vector function
void load_vector_ld(int n, long double* b, char* filename);

// Multiply a csr matrix with a vector x, and get the resulting vector y ,sum use kekan
// sum
void spmv_ld(int n, int* row_ptr, int* col_idx, long double* val, long double* x, long double* y)
{
    for (int i = 0; i < n; i++) {
        y[i]          = 0.0;
        long double c = 0.0;
        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            long double num = val[j] * x[col_idx[j]];
            long double z   = num - c;
            long double t   = y[i] + z;
            c               = (t - y[i]) - z;
            y[i]            = t;
        }
    }
}

// The complex number multiplication
void mul_ld(long double* v1, long double* v1i, long double* v2, long double* v2i, long double* v3, long double* v3i)
{
    *v3  = *v1 * *v2 - (*v1i * *v2i);
    *v3i = *v1i * *v2 + *v1 * *v2i;
}

// The complex number multiplication
void conj_mul_ld(long double* v1, long double* v1i, long double* v2, long double* v2i, long double* v3, long double* v3i)
{
    *v3  = *v1 * *v2 + (*v1i * *v2i);
    *v3i = *v1 * *v2i - *v1i * *v2;
}

// The complex number addition ,sum use kekan sum
void add_ld(long double* v1, long double* v1i, long double* sum, long double* sumi, long double* c, long double* ci)
{
    long double num = *v1;
    long double z   = num - *c;
    long double t   = *sum + z;
    *c              = (t - *sum) - z;
    *sum            = t;

    long double numi = *v1i;
    long double zi   = numi - *ci;
    long double ti   = *sumi + zi;
    *ci              = (ti - *sumi) - zi;
    *sumi            = ti;
}

// Multiply a csr matrix with two vector x and xi, and get the resulting vector y and yi
void spmv_complex_ld(int n, int* row_ptr, int* col_idx, long double* val, long double* vali, long double* x, long double* xi, long double* y,
                     long double* yi)
{
    long double tmp[2];
    long double c[2];
    for (int i = 0; i < n; i++) {
        y[i] = yi[i] = 0.0;
        c[0] = c[1] = 0.0;
        for (int j = row_ptr[i]; j < row_ptr[i + 1]; j++) {
            mul_ld(val + j, vali + j, x + col_idx[j], xi + col_idx[j], tmp, tmp + 1);
            add_ld(tmp, tmp + 1, y + i, yi + i, c, c + 1);
        }
    }
}

// Calculate the mode of complex number
void complex_modulus_squared_ld(long double* a, long double* ai, long double* l) { *l = *a * *a + (*ai * *ai); }

// Calculate the conjugate of complex number
void complex_conjugate_ld(long double* a, long double* ai, long double* b, long double* bi)
{
    *b  = *a;
    *bi = -*ai;
}

// Calculate the division of complex number
void complex_division_ld(long double* a, long double* ai, long double* b, long double* bi, long double* c, long double* ci)
{
    long double  tmp = 0.0;
    long double* l;
    l = &tmp;
    complex_modulus_squared_ld(b, bi, l);
    *c  = *a * *b + (*ai * *bi);
    *ci = *ai * *b - (*a * *bi);
    *c  = (*c) / (*l);
    *ci = (*ci) / (*l);
}

// Calculate the 2-norm of a vector ,sum use kekan sum
long double vec2norm_ld(long double* x, int n)
{
    long double sum = 0.0;
    long double c   = 0.0;
    for (int i = 0; i < n; i++) {
        long double num = x[i] * x[i];
        long double z   = num - c;
        long double t   = sum + z;
        c               = (t - sum) - z;
        sum             = t;
    }

    return sqrtl(sum);
}

// Calculate the 2-norm of a complex vector ,sum use kekan sum
void vec2norm_complex_ld(long double* x, long double* xi, long double* err, long double* erri, int n)
{
    long double sum[2] = {0, 0}, tmp[2];
    long double c[2];
    c[0] = c[1] = 0.0;

    for (int i = 0; i < n; i++) {
        conj_mul_ld(x + i, xi + i, x + i, xi + i, tmp, tmp + 1);
        add_ld(tmp, tmp + 1, sum, sum + 1, c, c + 1);
    }

    *err  = sqrtl(sum[0]);
    *erri = sqrtl(sum[1]);
}

long double max_check_ld(long double* x, int n)
{
    long double max = DBL_MIN;
    for (int i = 0; i < n; i++) {
        long double x_fabs = fabsl(x[i]);
        max                = max > x_fabs ? max : x_fabs;
    }
    return max;
}

long double max_check_complex_ld(long double* x, long double* xi, int n)
{
    long double max = DBL_MIN;
    for (int i = 0; i < n; i++) {
        long double x_fabs = x[i] * x[i] + xi[i] * xi[i];
        max                = max > x_fabs ? max : x_fabs;
    }
    return sqrtl(max);
}

// precision check using long double type
// validate the x
// answer1 = sqrtl(|| A*x - b ||)
// answer2 = || A*x - b || MAX
// answer3 = sqrtl(|| A*x - b ||/|| b ||)
// answer4 = MAX { |b - Ax|_i / |b_i| }
void check_correctness_ld(int n, int* row_ptr, int* col_idx, long double* val, long double* x, long double* b)
{
    long double* b_new   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b = (long double*)malloc(sizeof(long double) * n);
    // long double* r_b     = (long double*)malloc(sizeof(long double) * n);
    spmv_ld(n, row_ptr, col_idx, val, x, b_new);
    for (int i = 0; i < n; i++) {
        check_b[i] = b_new[i] - b[i];
        // r_b[i]     = fabsl(check_b[i]) / MAX(fabsl(b[i]), 1e-20);
    }

    long double answer1 = vec2norm_ld(check_b, n);
    long double answer2 = max_check_ld(check_b, n);
    long double answer3 = answer1 / vec2norm_ld(b, n);
    // long double answer4 = max_check_ld(r_b, n);

    fprintf(stdout, "LD-Check || b - Ax || 2             =  %12.6Le\n", answer1);
    fprintf(stdout, "LD-Check || b - Ax || MAX           =  %12.6Le\n", answer2);
    fprintf(stdout, "LD-Check || b - Ax || 2 / || b || 2 =  %12.6Le\n", answer3);
    // fprintf(stdout, "LD-Check MAX { |b - Ax|_i / |b_i| } =  %12.6Le\n", answer4);

    free(b_new);
    free(check_b);
    // free(r_b);
}

void check_correctness_ld_d2ld(int n, int* row_ptr, int* col_idx, double* val, double* x, double* b)
{
    //! Step 1: data type transformation: double -> long double
    int nnz = row_ptr[n] - row_ptr[0];
    // printf("nnz = %d\n", nnz);
    long double* val_ld = (long double*)malloc(sizeof(long double) * nnz);
    long double* x_ld   = (long double*)malloc(sizeof(long double) * n);
    long double* b_ld   = (long double*)malloc(sizeof(long double) * n);

    for (int i = 0; i < nnz; i++) val_ld[i] = (long double)val[i];

    for (int i = 0; i < n; i++) {
        x_ld[i] = (long double)x[i];
        b_ld[i] = (long double)b[i];
    }

    //! Step 2: Check
    long double* b_new   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b = (long double*)malloc(sizeof(long double) * n);
    // long double* r_b     = (long double*)malloc(sizeof(long double) * n);
    spmv_ld(n, row_ptr, col_idx, val_ld, x_ld, b_new);
    for (int i = 0; i < n; i++) {
        check_b[i] = b_new[i] - b_ld[i];
        // r_b[i]     = fabsl(check_b[i]) / MAX(fabsl(b_ld[i]), 1e-20);
    }

    long double answer1 = vec2norm_ld(check_b, n);
    long double answer2 = max_check_ld(check_b, n);
    long double answer3 = answer1 / vec2norm_ld(b_ld, n);
    // long double answer4 = max_check_ld(r_b, n);

    fprintf(stdout, "LD-Check || b - Ax || 2             =  %12.6Le\n", answer1);
    fprintf(stdout, "LD-Check || b - Ax || MAX           =  %12.6Le\n", answer2);
    fprintf(stdout, "LD-Check || b - Ax || 2 / || b || 2 =  %12.6Le\n", answer3);
    // fprintf(stdout, "LD-Check MAX { |b - Ax|_i / |b_i| } =  %12.6Le\n", answer4);

    //! Step 3: free memory
    free(val_ld);
    free(x_ld);
    free(b_ld);

    free(b_new);
    free(check_b);
    // free(r_b);
}

// precision check using long double type, solution vector already in long double
void check_correctness_ld_xld(int n, int* row_ptr, int* col_idx, double* val, long double* x, double* b)
{
    int nnz = row_ptr[n] - row_ptr[0];
    long double* val_ld = (long double*)malloc(sizeof(long double) * nnz);
    long double* b_ld   = (long double*)malloc(sizeof(long double) * n);
    for (int i = 0; i < nnz; i++) val_ld[i] = (long double)val[i];
    for (int i = 0; i < n; i++) b_ld[i] = (long double)b[i];

    long double* b_new   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b = (long double*)malloc(sizeof(long double) * n);
    spmv_ld(n, row_ptr, col_idx, val_ld, x, b_new);
    for (int i = 0; i < n; i++) check_b[i] = b_new[i] - b_ld[i];

    long double answer1 = vec2norm_ld(check_b, n);
    long double answer2 = max_check_ld(check_b, n);
    long double answer3 = answer1 / vec2norm_ld(b_ld, n);

    fprintf(stdout, "LD-Check || b - Ax || 2             =  %12.6Le\n", answer1);
    fprintf(stdout, "LD-Check || b - Ax || MAX           =  %12.6Le\n", answer2);
    fprintf(stdout, "LD-Check || b - Ax || 2 / || b || 2 =  %12.6Le\n", answer3);

    free(val_ld);
    free(b_ld);
    free(b_new);
    free(check_b);
}

void check_correctness_complex_ld(int n, int* row_ptr, int* col_idx, long double* val, long double* vali, long double* x, long double* xi,
                                  long double* b, long double* bi)
{
    long double* b_new     = (long double*)malloc(sizeof(long double) * n);
    long double* b_new_i   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b_i = (long double*)malloc(sizeof(long double) * n);

    long double* r_b     = (long double*)malloc(sizeof(long double) * n);
    long double* r_b_i   = (long double*)malloc(sizeof(long double) * n);
    // long double* rb_mode = (long double*)malloc(sizeof(long double) * n);
    // long double  tmp     = 0.0;
    // long double* l;
    // l = &tmp;

    spmv_complex_ld(n, row_ptr, col_idx, val, vali, x, xi, b_new, b_new_i);
    for (int i = 0; i < n; i++) {
        check_b[i]   = b_new[i] - b[i];
        check_b_i[i] = b_new_i[i] - bi[i];
        // complex_division_ld(&check_b[i], &check_b_i[i], &b[i], &bi[i], r_b, r_b_i);
        // complex_modulus_squared_ld(r_b, r_b_i, l);
        // rb_mode[i] = sqrtl(*l);
    }

    long double err_check_b  = 0.0;
    long double err_check_bi = 0.0;
    long double err_b        = 0.0;
    long double err_bi       = 0.0;

    vec2norm_complex_ld(check_b, check_b_i, &err_check_b, &err_check_bi, n);
    vec2norm_complex_ld(b, bi, &err_b, &err_bi, n);
    long double max_answer  = max_check_complex_ld(check_b, check_b_i, n);
    // long double max_answer2 = max_check_ld(rb_mode, n);

    fprintf(stdout, "LD-Check complex || b - Ax || 2             =  %12.6Le \n", err_check_b);
    fprintf(stdout, "LD-Check complex || b - Ax || MAX           =  %12.6Le \n", max_answer);
    fprintf(stdout, "LD-Check complex || b - Ax || 2 / || b || 2 =  %12.6Le \n", err_check_b / err_b);
    // fprintf(stdout, "LD-Check complex MAX { |b - Ax|_i / |b_i| } =  %12.6Le \n", max_answer2);

    free(b_new);
    free(b_new_i);
    free(check_b);
    free(check_b_i);
    free(r_b);
    free(r_b_i);
    // free(rb_mode);
}

void check_correctness_complex_ld_d2ld(int n, int* row_ptr, int* col_idx, double* val, double* vali, double* x, double* xi, double* b, double* bi)
{
    //! Step 1: data type transformation: double -> long double
    int nnz = row_ptr[n] - row_ptr[0];
    // printf("nnz = %d\n", nnz);
    long double* val_ld  = (long double*)malloc(sizeof(long double) * nnz);
    long double* vali_ld = (long double*)malloc(sizeof(long double) * nnz);
    long double* x_ld    = (long double*)malloc(sizeof(long double) * n);
    long double* xi_ld   = (long double*)malloc(sizeof(long double) * n);
    long double* b_ld    = (long double*)malloc(sizeof(long double) * n);
    long double* bi_ld   = (long double*)malloc(sizeof(long double) * n);

    for (int i = 0; i < nnz; i++) {
        val_ld[i]  = (long double)val[i];
        vali_ld[i] = (long double)vali[i];
    }

    for (int i = 0; i < n; i++) {
        x_ld[i]  = (long double)x[i];
        xi_ld[i] = (long double)xi[i];
        b_ld[i]  = (long double)b[i];
        bi_ld[i] = (long double)bi[i];
    }

    //! Step 2: Check
    long double* b_new     = (long double*)malloc(sizeof(long double) * n);
    long double* b_new_i   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b_i = (long double*)malloc(sizeof(long double) * n);

    long double* r_b     = (long double*)malloc(sizeof(long double) * n);
    long double* r_b_i   = (long double*)malloc(sizeof(long double) * n);
    // long double* rb_mode = (long double*)malloc(sizeof(long double) * n);
    // long double  tmp     = 0.0;
    // long double* l;
    // l = &tmp;

    spmv_complex_ld(n, row_ptr, col_idx, val_ld, vali_ld, x_ld, xi_ld, b_new, b_new_i);
    for (int i = 0; i < n; i++) {
        check_b[i]   = b_new[i] - b_ld[i];
        check_b_i[i] = b_new_i[i] - bi_ld[i];
        // complex_division_ld(&check_b[i], &check_b_i[i], &b_ld[i], &bi_ld[i], r_b, r_b_i);
        // complex_modulus_squared_ld(r_b, r_b_i, l);
        // rb_mode[i] = sqrtl(*l);
    }

    long double err_check_b  = 0.0;
    long double err_check_bi = 0.0;
    long double err_b        = 0.0;
    long double err_bi       = 0.0;

    vec2norm_complex_ld(check_b, check_b_i, &err_check_b, &err_check_bi, n);
    vec2norm_complex_ld(b_ld, bi_ld, &err_b, &err_bi, n);
    long double max_answer  = max_check_complex_ld(check_b, check_b_i, n);
    // long double max_answer2 = max_check_ld(rb_mode, n);

    fprintf(stdout, "LD-Check complex || b - Ax || 2             =  %12.6Le \n", err_check_b);
    fprintf(stdout, "LD-Check complex || b - Ax || MAX           =  %12.6Le \n", max_answer);
    fprintf(stdout, "LD-Check complex || b - Ax || 2 / || b || 2 =  %12.6Le \n", err_check_b / err_b);
    // fprintf(stdout, "LD-Check complex MAX { |b - Ax|_i / |b_i| } =  %12.6Le \n", max_answer2);

    //! Step 3: free memory
    free(val_ld);
    free(vali_ld);
    free(x_ld);
    free(xi_ld);
    free(b_ld);
    free(bi_ld);

    free(b_new);
    free(b_new_i);
    free(check_b);
    free(check_b_i);
    free(r_b);
    free(r_b_i);
    // free(rb_mode);
}

// precision check using long double type, complex version with solution already in long double
void check_correctness_complex_ld_xld(int n, int* row_ptr, int* col_idx, double* val, double* vali, long double* x, long double* xi,
                                      double* b, double* bi)
{
    int nnz = row_ptr[n] - row_ptr[0];
    long double* val_ld  = (long double*)malloc(sizeof(long double) * nnz);
    long double* vali_ld = (long double*)malloc(sizeof(long double) * nnz);
    long double* b_ld    = (long double*)malloc(sizeof(long double) * n);
    long double* bi_ld   = (long double*)malloc(sizeof(long double) * n);
    for (int i = 0; i < nnz; i++) {
        val_ld[i]  = (long double)val[i];
        vali_ld[i] = (long double)vali[i];
    }
    for (int i = 0; i < n; i++) {
        b_ld[i]  = (long double)b[i];
        bi_ld[i] = (long double)bi[i];
    }

    long double* b_new     = (long double*)malloc(sizeof(long double) * n);
    long double* b_new_i   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b   = (long double*)malloc(sizeof(long double) * n);
    long double* check_b_i = (long double*)malloc(sizeof(long double) * n);
    spmv_complex_ld(n, row_ptr, col_idx, val_ld, vali_ld, x, xi, b_new, b_new_i);
    for (int i = 0; i < n; i++) {
        check_b[i]   = b_new[i] - b_ld[i];
        check_b_i[i] = b_new_i[i] - bi_ld[i];
    }

    long double err_check_b = 0.0;
    long double err_check_bi = 0.0;
    long double err_b       = 0.0;
    long double err_bi      = 0.0;

    vec2norm_complex_ld(check_b, check_b_i, &err_check_b, &err_check_bi, n);
    vec2norm_complex_ld(b_ld, bi_ld, &err_b, &err_bi, n);
    long double max_answer = max_check_complex_ld(check_b, check_b_i, n);

    fprintf(stdout, "LD-Check complex || b - Ax || 2             =  %12.6Le \n", err_check_b);
    fprintf(stdout, "LD-Check complex || b - Ax || MAX           =  %12.6Le \n", max_answer);
    fprintf(stdout, "LD-Check complex || b - Ax || 2 / || b || 2 =  %12.6Le \n", err_check_b / err_b);

    free(val_ld);
    free(vali_ld);
    free(b_ld);
    free(bi_ld);
    free(b_new);
    free(b_new_i);
    free(check_b);
    free(check_b_i);
}

// store x to a file
void store_x_ld(int n, long double* x, char* filename)
{
    FILE* p = fopen(filename, "w");
    if (!p) { perror(filename); exit(EXIT_FAILURE); }
    fprintf(p, "%d\n", n);
    for (int i = 0; i < n; i++) fprintf(p, "%.18Le\n", x[i]);
    fclose(p);
}

// load right-hand side vector b
void load_vector_ld(int n, long double* b, char* filename)
{
    FILE* p = fopen(filename, "r");
    int   n_right;
    // MM_typecode matcode;

    char line[MM_MAX_LINE_LENGTH];
    fgets(line, MM_MAX_LINE_LENGTH, p);
    int r = 0;

    if (line[0] == '%') {
        r = fscanf(p, "%d", &n_right);
    } else {
        n_right = atoi(line);
    }

    if (n_right != n) {
        fclose(p);
        fprintf(stdout, "Invalid size of vector.\n");
        return;
    }
    for (int i = 0; i < n_right; i++) r = fscanf(p, "%Lf", &b[i]);
    fclose(p);
}


#ifndef SC26_DISABLE_FLOAT128

#include <quadmath.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Quad-precision residual checks for the existing double-valued solver API.
 * Matrix, solution, and right-hand-side entries are promoted on each
 * arithmetic operation, so this does not allocate a second quad matrix.
 */

static __float128 vec2norm_q_d(const double *x, int n)
{
    __float128 sum = 0.0Q;
    __float128 compensation = 0.0Q;
    int i;
    for (i = 0; i < n; ++i) {
        __float128 term = (__float128)x[i] * (__float128)x[i];
        __float128 corrected = term - compensation;
        __float128 next = sum + corrected;
        compensation = (next - sum) - corrected;
        sum = next;
    }
    return sqrtq(sum);
}

static __float128 vec2norm_complex_q_d(const double *x, const double *xi, int n)
{
    __float128 sum = 0.0Q;
    __float128 compensation = 0.0Q;
    int i;
    for (i = 0; i < n; ++i) {
        __float128 re = (__float128)x[i];
        __float128 im = (__float128)xi[i];
        __float128 term = re * re + im * im;
        __float128 corrected = term - compensation;
        __float128 next = sum + corrected;
        compensation = (next - sum) - corrected;
        sum = next;
    }
    return sqrtq(sum);
}

static __float128 max_check_q_d(const __float128 *x, int n)
{
    __float128 maximum = 0.0Q;
    int i;
    for (i = 0; i < n; ++i) {
        __float128 value = fabsq(x[i]);
        if (value > maximum) maximum = value;
    }
    return maximum;
}

static __float128 max_check_complex_q(const __float128 *x, const __float128 *xi, int n)
{
    __float128 maximum = 0.0Q;
    int i;
    for (i = 0; i < n; ++i) {
        __float128 re = x[i];
        __float128 im = xi[i];
        __float128 value = sqrtq(re * re + im * im);
        if (value > maximum) maximum = value;
    }
    return maximum;
}

static void spmv_q_d(const int n, const int *row_ptr, const int *col_idx,
                     const double *val, const double *x, __float128 *y)
{
    int i;
    for (i = 0; i < n; ++i) {
        __float128 sum = 0.0Q;
        __float128 compensation = 0.0Q;
        int j;
        for (j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            __float128 term = (__float128)val[j] * (__float128)x[col_idx[j]];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        y[i] = sum;
    }
}

static void spmv_complex_q_d(const int n, const int *row_ptr, const int *col_idx,
                             const double *val, const double *vali,
                             const double *x, const double *xi,
                             __float128 *y, __float128 *yi)
{
    int i;
    for (i = 0; i < n; ++i) {
        __float128 sum_re = 0.0Q, sum_im = 0.0Q;
        __float128 compensation_re = 0.0Q, compensation_im = 0.0Q;
        int j;
        for (j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            __float128 ar = (__float128)val[j];
            __float128 ai = (__float128)vali[j];
            __float128 xr = (__float128)x[col_idx[j]];
            __float128 xi_value = (__float128)xi[col_idx[j]];
            __float128 term_re = ar * xr - ai * xi_value;
            __float128 term_im = ai * xr + ar * xi_value;
            __float128 corrected_re = term_re - compensation_re;
            __float128 corrected_im = term_im - compensation_im;
            __float128 next_re = sum_re + corrected_re;
            __float128 next_im = sum_im + corrected_im;
            compensation_re = (next_re - sum_re) - corrected_re;
            compensation_im = (next_im - sum_im) - corrected_im;
            sum_re = next_re;
            sum_im = next_im;
        }
        y[i] = sum_re;
        yi[i] = sum_im;
    }
}

static void print_q_check(const char *label, __float128 value)
{
    char text[128];
    quadmath_snprintf(text, sizeof(text), "%12.6Qe", value);
    fprintf(stdout, "%s%s\n", label, text);
}

/* Check a double-valued system using quad-precision arithmetic. */
void check_correctness_q_d2q(int n, const int *row_ptr, const int *col_idx,
                                    const double *val, const double *x, const double *b)
{
    __float128 *b_new = (__float128 *)malloc(sizeof(*b_new) * (size_t)n);
    __float128 *residual = (__float128 *)malloc(sizeof(*residual) * (size_t)n);
    __float128 answer1, answer2, answer3;
    int i;
    if (!b_new || !residual) {
        fprintf(stdout, "Q-Check allocation failed.\n");
        free(b_new);
        free(residual);
        return;
    }

    spmv_q_d(n, row_ptr, col_idx, val, x, b_new);
    for (i = 0; i < n; ++i) residual[i] = b_new[i] - (__float128)b[i];
    {
        __float128 sum = 0.0Q, compensation = 0.0Q;
        for (i = 0; i < n; ++i) {
            __float128 term = residual[i] * residual[i];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        answer1 = sqrtq(sum);
    }
    answer2 = max_check_q_d(residual, n);
    answer3 = answer1 / vec2norm_q_d(b, n);

    print_q_check("Q-Check || b - Ax || 2             =  ", answer1);
    print_q_check("Q-Check || b - Ax || MAX           =  ", answer2);
    print_q_check("Q-Check || b - Ax || 2 / || b || 2 =  ", answer3);
    free(b_new);
    free(residual);
}

void check_correctness_complex_q_d2q(int n, const int *row_ptr, const int *col_idx,
                                            const double *val, const double *vali,
                                            const double *x, const double *xi,
                                            const double *b, const double *bi)
{
    __float128 *b_new = (__float128 *)malloc(sizeof(*b_new) * (size_t)n);
    __float128 *b_new_i = (__float128 *)malloc(sizeof(*b_new_i) * (size_t)n);
    __float128 *residual = (__float128 *)malloc(sizeof(*residual) * (size_t)n);
    __float128 *residual_i = (__float128 *)malloc(sizeof(*residual_i) * (size_t)n);
    __float128 answer1, answer2, answer3;
    int i;
    if (!b_new || !b_new_i || !residual || !residual_i) {
        fprintf(stdout, "Q-Check complex allocation failed.\n");
        free(b_new);
        free(b_new_i);
        free(residual);
        free(residual_i);
        return;
    }

    spmv_complex_q_d(n, row_ptr, col_idx, val, vali, x, xi, b_new, b_new_i);
    for (i = 0; i < n; ++i) {
        residual[i] = b_new[i] - (__float128)b[i];
        residual_i[i] = b_new_i[i] - (__float128)bi[i];
    }
    answer1 = 0.0Q;
    {
        __float128 sum = 0.0Q, compensation = 0.0Q;
        for (i = 0; i < n; ++i) {
            __float128 term = residual[i] * residual[i] + residual_i[i] * residual_i[i];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        answer1 = sqrtq(sum);
    }
    answer2 = max_check_complex_q(residual, residual_i, n);
    answer3 = answer1 / vec2norm_complex_q_d(b, bi, n);

    print_q_check("Q-Check complex || b - Ax || 2             =  ", answer1);
    print_q_check("Q-Check complex || b - Ax || MAX           =  ", answer2);
    print_q_check("Q-Check complex || b - Ax || 2 / || b || 2 =  ", answer3);
    free(b_new);
    free(b_new_i);
    free(residual);
    free(residual_i);
}

/*
 * Quad-precision residual checks with the solution already held in long double.
 * Matrix and right-hand-side entries are double and are promoted on each
 * arithmetic operation; the solution vector stays in long double.
 */

static void spmv_q_ldx(const int n, const int *row_ptr, const int *col_idx,
                       const double *val, const long double *x, __float128 *y)
{
    int i;
    for (i = 0; i < n; ++i) {
        __float128 sum = 0.0Q;
        __float128 compensation = 0.0Q;
        int j;
        for (j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            __float128 term = (__float128)val[j] * (__float128)x[col_idx[j]];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        y[i] = sum;
    }
}

static void spmv_complex_q_ldx(const int n, const int *row_ptr, const int *col_idx,
                               const double *val, const double *vali,
                               const long double *x, const long double *xi,
                               __float128 *y, __float128 *yi)
{
    int i;
    for (i = 0; i < n; ++i) {
        __float128 sum_re = 0.0Q, sum_im = 0.0Q;
        __float128 compensation_re = 0.0Q, compensation_im = 0.0Q;
        int j;
        for (j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            __float128 ar = (__float128)val[j];
            __float128 ai = (__float128)vali[j];
            __float128 xr = (__float128)x[col_idx[j]];
            __float128 xi_value = (__float128)xi[col_idx[j]];
            __float128 term_re = ar * xr - ai * xi_value;
            __float128 term_im = ai * xr + ar * xi_value;
            __float128 corrected_re = term_re - compensation_re;
            __float128 corrected_im = term_im - compensation_im;
            __float128 next_re = sum_re + corrected_re;
            __float128 next_im = sum_im + corrected_im;
            compensation_re = (next_re - sum_re) - corrected_re;
            compensation_im = (next_im - sum_im) - corrected_im;
            sum_re = next_re;
            sum_im = next_im;
        }
        y[i] = sum_re;
        yi[i] = sum_im;
    }
}

void check_correctness_q_ldx(int n, const int *row_ptr, const int *col_idx,
                             const double *val, const long double *x, const double *b)
{
    __float128 *b_new = (__float128 *)malloc(sizeof(*b_new) * (size_t)n);
    __float128 *residual = (__float128 *)malloc(sizeof(*residual) * (size_t)n);
    __float128 answer1, answer2, answer3;
    int i;
    if (!b_new || !residual) {
        fprintf(stdout, "Q-Check(ld x) allocation failed.\n");
        free(b_new);
        free(residual);
        return;
    }

    spmv_q_ldx(n, row_ptr, col_idx, val, x, b_new);
    for (i = 0; i < n; ++i) residual[i] = b_new[i] - (__float128)b[i];
    {
        __float128 sum = 0.0Q, compensation = 0.0Q;
        for (i = 0; i < n; ++i) {
            __float128 term = residual[i] * residual[i];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        answer1 = sqrtq(sum);
    }
    answer2 = max_check_q_d(residual, n);
    answer3 = answer1 / vec2norm_q_d(b, n);

    print_q_check("Qx-Check || b - Ax || 2             =  ", answer1);
    print_q_check("Qx-Check || b - Ax || MAX           =  ", answer2);
    print_q_check("Qx-Check || b - Ax || 2 / || b || 2 =  ", answer3);
    free(b_new);
    free(residual);
}

void check_correctness_complex_q_ldx(int n, const int *row_ptr, const int *col_idx,
                                     const double *val, const double *vali,
                                     const long double *x, const long double *xi,
                                     const double *b, const double *bi)
{
    __float128 *b_new = (__float128 *)malloc(sizeof(*b_new) * (size_t)n);
    __float128 *b_new_i = (__float128 *)malloc(sizeof(*b_new_i) * (size_t)n);
    __float128 *residual = (__float128 *)malloc(sizeof(*residual) * (size_t)n);
    __float128 *residual_i = (__float128 *)malloc(sizeof(*residual_i) * (size_t)n);
    __float128 answer1, answer2, answer3;
    int i;
    if (!b_new || !b_new_i || !residual || !residual_i) {
        fprintf(stdout, "Q-Check complex (ld x) allocation failed.\n");
        free(b_new);
        free(b_new_i);
        free(residual);
        free(residual_i);
        return;
    }

    spmv_complex_q_ldx(n, row_ptr, col_idx, val, vali, x, xi, b_new, b_new_i);
    for (i = 0; i < n; ++i) {
        residual[i] = b_new[i] - (__float128)b[i];
        residual_i[i] = b_new_i[i] - (__float128)bi[i];
    }
    {
        __float128 sum = 0.0Q, compensation = 0.0Q;
        for (i = 0; i < n; ++i) {
            __float128 term = residual[i] * residual[i] + residual_i[i] * residual_i[i];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        answer1 = sqrtq(sum);
    }
    answer2 = max_check_complex_q(residual, residual_i, n);
    answer3 = answer1 / vec2norm_complex_q_d(b, bi, n);

    print_q_check("Qx-Check complex || b - Ax || 2             =  ", answer1);
    print_q_check("Qx-Check complex || b - Ax || MAX           =  ", answer2);
    print_q_check("Qx-Check complex || b - Ax || 2 / || b || 2 =  ", answer3);
    free(b_new);
    free(b_new_i);
    free(residual);
    free(residual_i);
}

/*
 * Quad-precision residual checks with the solution already held in __float128.
 * Matrix and right-hand-side entries are double and are promoted on each
 * arithmetic operation; the solution vector stays in __float128.
 */

static void spmv_q_xq(const int n, const int *row_ptr, const int *col_idx,
                      const double *val, const __float128 *x, __float128 *y)
{
    int i;
    for (i = 0; i < n; ++i) {
        __float128 sum = 0.0Q;
        __float128 compensation = 0.0Q;
        int j;
        for (j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            __float128 term = (__float128)val[j] * x[col_idx[j]];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        y[i] = sum;
    }
}

static void spmv_complex_q_xq(const int n, const int *row_ptr, const int *col_idx,
                              const double *val, const double *vali,
                              const __float128 *x, const __float128 *xi,
                              __float128 *y, __float128 *yi)
{
    int i;
    for (i = 0; i < n; ++i) {
        __float128 sum_re = 0.0Q, sum_im = 0.0Q;
        __float128 compensation_re = 0.0Q, compensation_im = 0.0Q;
        int j;
        for (j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
            __float128 ar = (__float128)val[j];
            __float128 ai = (__float128)vali[j];
            __float128 xr = x[col_idx[j]];
            __float128 xi_value = xi[col_idx[j]];
            __float128 term_re = ar * xr - ai * xi_value;
            __float128 term_im = ai * xr + ar * xi_value;
            __float128 corrected_re = term_re - compensation_re;
            __float128 corrected_im = term_im - compensation_im;
            __float128 next_re = sum_re + corrected_re;
            __float128 next_im = sum_im + corrected_im;
            compensation_re = (next_re - sum_re) - corrected_re;
            compensation_im = (next_im - sum_im) - corrected_im;
            sum_re = next_re;
            sum_im = next_im;
        }
        y[i] = sum_re;
        yi[i] = sum_im;
    }
}

long double check_correctness_q_xq(int n, const int *row_ptr, const int *col_idx,
                            const double *val, const __float128 *x, const double *b)
{
    __float128 *b_new = (__float128 *)malloc(sizeof(*b_new) * (size_t)n);
    __float128 *residual = (__float128 *)malloc(sizeof(*residual) * (size_t)n);
    __float128 answer1, answer2, answer3;
    int i;
    if (!b_new || !residual) {
        fprintf(stdout, "Q-Check allocation failed.\n");
        free(b_new);
        free(residual);
        return INFINITY;
    }

    spmv_q_xq(n, row_ptr, col_idx, val, x, b_new);
    for (i = 0; i < n; ++i) residual[i] = b_new[i] - (__float128)b[i];
    {
        __float128 sum = 0.0Q, compensation = 0.0Q;
        for (i = 0; i < n; ++i) {
            __float128 term = residual[i] * residual[i];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        answer1 = sqrtq(sum);
    }
    answer2 = max_check_q_d(residual, n);
    answer3 = answer1 / vec2norm_q_d(b, n);

    print_q_check("Q-Check || b - Ax || 2             =  ", answer1);
    print_q_check("Q-Check || b - Ax || MAX           =  ", answer2);
    print_q_check("Q-Check || b - Ax || 2 / || b || 2 =  ", answer3);
    free(b_new);
    free(residual);
    return (long double)answer3;
}

long double check_correctness_complex_q_xq(int n, const int *row_ptr, const int *col_idx,
                                    const double *val, const double *vali,
                                    const __float128 *x, const __float128 *xi,
                                    const double *b, const double *bi)
{
    __float128 *b_new = (__float128 *)malloc(sizeof(*b_new) * (size_t)n);
    __float128 *b_new_i = (__float128 *)malloc(sizeof(*b_new_i) * (size_t)n);
    __float128 *residual = (__float128 *)malloc(sizeof(*residual) * (size_t)n);
    __float128 *residual_i = (__float128 *)malloc(sizeof(*residual_i) * (size_t)n);
    __float128 answer1, answer2, answer3;
    int i;
    if (!b_new || !b_new_i || !residual || !residual_i) {
        fprintf(stdout, "Q-Check complex allocation failed.\n");
        free(b_new);
        free(b_new_i);
        free(residual);
        free(residual_i);
        return INFINITY;
    }

    spmv_complex_q_xq(n, row_ptr, col_idx, val, vali, x, xi, b_new, b_new_i);
    for (i = 0; i < n; ++i) {
        residual[i] = b_new[i] - (__float128)b[i];
        residual_i[i] = b_new_i[i] - (__float128)bi[i];
    }
    {
        __float128 sum = 0.0Q, compensation = 0.0Q;
        for (i = 0; i < n; ++i) {
            __float128 term = residual[i] * residual[i] + residual_i[i] * residual_i[i];
            __float128 corrected = term - compensation;
            __float128 next = sum + corrected;
            compensation = (next - sum) - corrected;
            sum = next;
        }
        answer1 = sqrtq(sum);
    }
    answer2 = max_check_complex_q(residual, residual_i, n);
    answer3 = answer1 / vec2norm_complex_q_d(b, bi, n);

    print_q_check("Q-Check complex || b - Ax || 2             =  ", answer1);
    print_q_check("Q-Check complex || b - Ax || MAX           =  ", answer2);
    print_q_check("Q-Check complex || b - Ax || 2 / || b || 2 =  ", answer3);
    free(b_new);
    free(b_new_i);
    free(residual);
    free(residual_i);
    return (long double)answer3;
}

#endif
