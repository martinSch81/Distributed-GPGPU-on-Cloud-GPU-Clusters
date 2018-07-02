// Matrix multiplication.
// author: Schuchardt Martin, csap9442

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#ifndef ACC_DEVICE
#define ACC_DEVICE 1
#endif

#ifndef WORKGROUPSIZE
#define WORKGROUPSIZE 64
#endif

// TILESIZE*TILESIZE has to be <= 1024 for c++AMP and <= 256 for AMD Caicos and Intel HD Graphics
#ifndef TILESIZE
#define TILESIZE 16
#endif

#ifndef VERIFY
#define VERIFY 1
#endif

#define _INT    0
#define _FLOAT  1
#define _DOUBLE 2

#if NUMBER_TYPE == _FLOAT
#define SIZEOF_CL_REAL sizeof(cl_float)
#define SIZEOF_REAL sizeof(float)
#define REAL_STRING    "float"
#define REAL           float
#elif NUMBER_TYPE == _DOUBLE
#define SIZEOF_CL_REAL sizeof(cl_double)
#define SIZEOF_REAL sizeof(double)
#define REAL_STRING    "double"
#define REAL           double
#else
#define SIZEOF_CL_REAL sizeof(cl_int)
#define SIZEOF_REAL sizeof(int)
#define REAL_STRING    "int"
#define REAL           int
#endif

// switch: disable optimizations: "-cl-opt-disable", enable optimizations: "" (default)
#ifndef CL_OPTIMIZATIONS
#define CL_OPTIMIZATIONS ""
#endif


extern int N;
extern float EPSILON;

void initMatrices(REAL *A, REAL *B);
void zeroMatrix(REAL *C);
void testResults(REAL *should, REAL *is);
