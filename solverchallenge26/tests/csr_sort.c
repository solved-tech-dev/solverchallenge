#include "solverchallenge26/system_loader.h"
#include <assert.h>
int main(void) {
 int rp[]={0,3,4,4}, ci[]={2,0,1,1};
 double re[]={20,0,10,11}, im[]={-20,0,-10,-11};
 assert(!sc26_sort_csr_rows(3,rp,ci,re,im));
 assert(ci[0]==0 && ci[1]==1 && ci[2]==2 && re[1]==10 && im[2]==-20);
 assert(!sc26_sort_csr_rows(3,rp,ci,re,im));
 ci[1]=0; assert(sc26_sort_csr_rows(3,rp,ci,re,im)!=0);
 ci[1]=3; assert(sc26_sort_csr_rows(3,rp,ci,re,im)!=0);
 int one[]={0,1}, c[]={0}; double v[]={7};
 assert(!sc26_sort_csr_rows(1,one,c,v,0) && v[0]==7);
 return 0;
}
