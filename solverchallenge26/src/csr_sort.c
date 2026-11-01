#include "solverchallenge26/system_loader.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct { int col; double re, im; } Entry;
static int compare_entry(const void *a, const void *b)
{
    int x = ((const Entry *)a)->col, y = ((const Entry *)b)->col;
    return (x > y) - (x < y);
}
int sc26_sort_csr_rows(int n, const int *row, int *col, double *re, double *im)
{
    int maxlen = 0;
    if (n < 0 || !row || !col || !re || row[0] != 0) return -1;
    for (int i=0; i<n; ++i) {
        if (row[i+1] < row[i]) return -1;
        if (row[i+1]-row[i] > maxlen) maxlen=row[i+1]-row[i];
    }
    Entry *tmp = maxlen ? malloc((size_t)maxlen*sizeof(*tmp)) : NULL;
    if (maxlen && !tmp) return -1;
    for (int i=0; i<n; ++i) {
        int begin=row[i], end=row[i+1], sorted=1;
        for (int j=begin; j<end; ++j) {
            if (col[j]<0 || col[j]>=n) { free(tmp); return -1; }
            if (j>begin && col[j]<=col[j-1]) sorted=0;
        }
        if (sorted) continue;
        for (int j=begin; j<end; ++j) {
            tmp[j-begin]=(Entry){col[j],re[j],im?im[j]:0};
        }
        qsort(tmp,(size_t)(end-begin),sizeof(*tmp),compare_entry);
        for (int j=1; j<end-begin; ++j) {
            if (tmp[j].col==tmp[j-1].col) {
                fprintf(stderr,"Duplicate CSR column in row %d\n",i);
                free(tmp); return -1;
            }
        }
        for (int j=begin; j<end; ++j) {
            col[j]=tmp[j-begin].col; re[j]=tmp[j-begin].re;
            if (im) im[j]=tmp[j-begin].im;
        }
    }
    free(tmp); return 0;
}
