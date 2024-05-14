#ifndef __EXP_ZERO_CHU__
#define __EXP_ZERO_CHU__

#include <stdio.h>
extern "C" float *matAdd(float *a,float *b,int length);
extern "C" void kernelCpp(int* h_a, int N, int threadsPerBlock, int blocksPerGrid, int* d_a, cudaStream_t stream1);
#endif
