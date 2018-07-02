// Matrix multiplication.
// author: Schuchardt Martin, csap9442

#include <iostream>

#ifndef VERIFY
#define VERIFY 1
#endif

extern int N;
extern float EPSILON;

void initMatrices(int *A, int *B);
void testResults(int *should, int *is);
void printMatrix(int*, int, int);
