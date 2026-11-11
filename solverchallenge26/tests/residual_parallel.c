#include "solverchallenge26/diagnostics.h"

#include <assert.h>
#include <math.h>
#include <omp.h>
#include <stdlib.h>

int main(void)
{
    const int n = 2053;
    const int width = 17;
    int* row = malloc(((size_t)n + 1) * sizeof(*row));
    int* column = malloc((size_t)n * width * sizeof(*column));
    double* values = malloc((size_t)n * width * sizeof(*values));
    double* imaginary = malloc((size_t)n * width * sizeof(*imaginary));
    double* rhs = malloc((size_t)n * sizeof(*rhs));
    double* rhs_im = malloc((size_t)n * sizeof(*rhs_im));
    __float128* x = malloc((size_t)n * sizeof(*x));
    __float128* x_im = malloc((size_t)n * sizeof(*x_im));
    long double serial_real, parallel_real, serial_complex, parallel_complex;
    if (!row || !column || !values || !imaginary || !rhs || !rhs_im || !x || !x_im)
        return 1;

    for (int i = 0; i < n; ++i) {
        row[i] = i * width;
        rhs[i] = (i + 1) * 0.03125;
        rhs_im[i] = (i - 8) * 0.0625;
        x[i] = (__float128)(i - 20) / 7.0Q;
        x_im[i] = (__float128)(i + 3) / 13.0Q;
        for (int j = 0; j < width; ++j) {
            int entry = i * width + j;
            column[entry] = (i * 11 + j * 29) % n;
            values[entry] = (j % 2 ? -1.0 : 1.0) * ldexp((j + 1) / 19.0, j % 7 - 3);
            imaginary[entry] = (j - 5) / 23.0;
        }
    }
    row[n] = n * width;
    omp_set_dynamic(0);
    omp_set_num_threads(1);
    serial_real = check_correctness_q_xq(n, row, column, values, x, rhs);
    serial_complex = check_correctness_complex_q_xq(n, row, column, values, imaginary, x, x_im, rhs, rhs_im);
    omp_set_num_threads(4);
    parallel_real = check_correctness_q_xq(n, row, column, values, x, rhs);
    parallel_complex = check_correctness_complex_q_xq(n, row, column, values, imaginary, x, x_im, rhs, rhs_im);
    int passed = isfinite(serial_real) && isfinite(serial_complex) &&
        serial_real == parallel_real && serial_complex == parallel_complex;

    free(row); free(column); free(values); free(imaginary);
    free(rhs); free(rhs_im); free(x); free(x_im);
    return passed ? 0 : 1;
}
