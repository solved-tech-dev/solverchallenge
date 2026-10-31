#ifndef SC26_PUBLIC_RESIDUAL_CHECK_H
#define SC26_PUBLIC_RESIDUAL_CHECK_H

/* Double, long-double, and float128 residual checks. */
#include "runtime_support.h"

#ifdef __cplusplus
extern "C" {
#endif

void check_correctness(int n, int *row_ptr, int *col_idx, double *val,
                       double *x, double *b);
void check_correctness_complex(int n, int *row_ptr, int *col_idx, double *val,
                               double *val_im, double *x, double *x_im,
                               double *b, double *b_im);
void check_correctness_ld_d2ld(int n, int *row_ptr, int *col_idx, double *val,
                               double *x, double *b);
void check_correctness_complex_ld_d2ld(int n, int *row_ptr, int *col_idx,
                                       double *val, double *val_im, double *x,
                                       double *x_im, double *b, double *b_im);
void check_correctness_ld_xld(int n, int *row_ptr, int *col_idx, double *val,
                              long double *x, double *b);
void check_correctness_complex_ld_xld(int n, int *row_ptr, int *col_idx,
                                      double *val, double *val_im,
                                      long double *x, long double *x_im,
                                      double *b, double *b_im);
#ifndef SC26_DISABLE_FLOAT128
void check_correctness_q_d2q(int n, const int *row_ptr, const int *col_idx,
                             const double *val, const double *x,
                             const double *b);
void check_correctness_complex_q_d2q(int n, const int *row_ptr,
                                     const int *col_idx, const double *val,
                                     const double *val_im, const double *x,
                                     const double *x_im, const double *b,
                                     const double *b_im);
void check_correctness_q_ldx(int n, const int *row_ptr, const int *col_idx,
                             const double *val, const long double *x,
                             const double *b);
void check_correctness_complex_q_ldx(int n, const int *row_ptr,
                                     const int *col_idx, const double *val,
                                     const double *val_im, const long double *x,
                                     const long double *x_im, const double *b,
                                     const double *b_im);
void check_correctness_q_xq(int n, const int *row_ptr, const int *col_idx,
                            const double *val, const __float128 *x,
                            const double *b);
void check_correctness_complex_q_xq(int n, const int *row_ptr,
                                    const int *col_idx, const double *val,
                                    const double *val_im, const __float128 *x,
                                    const __float128 *x_im, const double *b,
                                    const double *b_im);
#endif

#ifdef __cplusplus
}
#endif

#endif
