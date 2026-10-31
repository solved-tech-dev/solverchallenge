/*
 * Input view passed from the SolverChallenge26 harness to a problem solver.
 * The arrays remain owned by the harness and are valid for the call.
 */
#ifndef SC26_PUBLIC_SOLVER_INPUT_H
#define SC26_PUBLIC_SOLVER_INPUT_H

#ifdef MPI_USE
#include "mpi.h"
#endif

#ifdef SC26_SOLUTION_FP128
#ifdef SC26_DISABLE_FLOAT128
#error SC26_SOLUTION_FP128 requires float128 residual checks
#endif
typedef __float128 SC26_SolutionScalar;
#else
typedef double SC26_SolutionScalar;
#endif

struct SC26_SolverInput {
    int global_n;
    int local_n;
    int row_begin;
    int rhs_count;
    const int* row_ptr;
    const int* col_idx;
    const double* val;
    const double* val_im;
    SC26_SolutionScalar* x;
    SC26_SolutionScalar* x_im;
    const double* b;
    const double* b_im;
    SC26_SolutionScalar* x_all;
    SC26_SolutionScalar* x_im_all;
    const double* b_all;
    const double* b_im_all;
#ifdef MPI_USE
    MPI_Comm comm;
#endif
};

#endif
