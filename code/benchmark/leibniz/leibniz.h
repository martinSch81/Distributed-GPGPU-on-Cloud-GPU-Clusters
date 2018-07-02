// Calculation of PI using Leibniz formula
// author: Schuchardt Martin, csap9442

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <limits.h>

#ifndef ACC_DEVICE
#define ACC_DEVICE 1
#endif

#ifndef WORKSIZE
#define WORKSIZE 64
#endif

#ifndef VERIFY
#define VERIFY 1
#endif

#define _FLOAT  1
#define _DOUBLE 2

#if NUMBER_TYPE == _FLOAT
#define SIZEOF_CL_REAL sizeof(cl_float)
#define SIZEOF_REAL sizeof(float)
#define REAL_STRING    "float"
#define REAL           float
#else
#define SIZEOF_CL_REAL sizeof(cl_double)
#define SIZEOF_REAL sizeof(double)
#define REAL_STRING    "double"
#define REAL           double
#endif

// switch: disable optimizations: "-cl-opt-disable", enable optimizations: "" (default)
#ifndef CL_OPTIMIZATIONS
#define CL_OPTIMIZATIONS ""
#endif


extern unsigned long long STEPS;
extern int REDUCTION_WORKGROUP;
extern double PI;
extern double EPSILON;

void verifyPI(REAL is);