// Matrix multiplication
// author: Schuchardt Martin, csap9442

#pragma once
#include <iostream>
#include <string>
#include "../utils/cl_utils.h"


#ifndef VERIFY
#define VERIFY 1
#endif

#ifndef DATA_TYPE
#define DATA_TYPE int
#define DATA_TYPE_CL cl_int
#define DATA_TYPE_STRING "int"
//#define DATA_TYPE float
//#define DATA_TYPE_CL cl_float
//#define DATA_TYPE_STRING "float"
//#define DATA_TYPE double
//#define DATA_TYPE_CL cl_double
//#define DATA_TYPE_STRING "double"
#endif


typedef struct {
	cl_context ctx;
	cl_command_queue queue;
	cl_device_id id;
	cl_program prog;
	cl_kernel kernel;
} ocl_mgmt;
ocl_mgmt cl;

typedef struct {
	size_t globalWorkGroupSize[2];
	size_t localWorkGroupSize[2];
	cl_mem A;
	cl_mem B;
	cl_mem C;
	cl_mem size;
} ocl_parameters;
ocl_parameters cl_param;



const float EPSILON = 0.0000000001f;
void multiplyChunkCPU(const DATA_TYPE *A, const int ROWS, const int COLUMNS, const DATA_TYPE *B, DATA_TYPE *C);
void multiplyChunkCL(const DATA_TYPE_CL *A, const int ROWS, const int COLUMNS, const DATA_TYPE_CL *B, DATA_TYPE_CL *C, unsigned acc_device);

#include "mmul.cl"
#include "mmul.tpp"
