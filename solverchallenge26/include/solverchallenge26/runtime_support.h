#ifndef SC26_PUBLIC_RUNTIME_SUPPORT_H
#define SC26_PUBLIC_RUNTIME_SUPPORT_H

/* Timing, memory, vector I/O, and double-precision kernels. */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void mem_usage(void);
void print_help(void);
double GetCurrentTime(void);
void store_x(int n, double *x, char *filename);
void store_x_complex(int n, double *x, double *x_im, char *filename);
void store_x_ld(int n, long double *x, char *filename);
void store_x_complex_ld(int n, long double *x, long double *x_im, char *filename);
void load_vector(int n, double *b, char *filename);
void load_b_complex(int n, double *b, double *b_im, char *filename);
void spmv(int n, int *row_ptr, int *col_idx, double *val, double *x, double *y);
void spmv_complex(int n, int *row_ptr, int *col_idx, double *val, double *val_im,
                  double *x, double *x_im, double *y, double *y_im);
double vec2norm(double *x, int n);
void vec2norm_complex(double *x, double *x_im, double *err, double *err_im, int n);
double max_check(double *x, int n);
double max_check_complex(double *x, double *x_im, int n);

#ifdef __cplusplus
}
#endif

#endif
