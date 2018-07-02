// Micro Benchmark using
//   int: +, -, *, /, %
//   float: +, -, *, /, sin, tan, cos, log, sqrt, pow
//   double: +, -, *, /, sin, tan, cos, log, sqrt, pow
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

#ifndef VERIFY
#define VERIFY 1
#endif

// switch: disable optimizations: "-cl-opt-disable", enable optimizations: "" (default)
#ifndef CL_OPTIMIZATIONS
#define CL_OPTIMIZATIONS ""
#endif


extern int THREADS;
extern int ITERS;
extern float EPSILON;


// calculate only the first and last 10 results to compare
void verifyInt(int *is);
void verifyFloat(float *is);
void verifyDouble(double *is);
