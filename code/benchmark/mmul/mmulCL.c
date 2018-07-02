// Matrix multiplication.
//  -naive version
//  -tiled version
// author: Schuchardt Martin, csap9442
// compile: gcc -O3 -std=c99 -Wall -Werror -lm mmul.c mmulCL.c -I../include -lOpenCL -o mmulCL_D1_int -DNUMBER_TYPE=0 -DWORKGROUPSIZE=64 -DTILESIZE=16  -DACC_DEVICE=1 -DVERIFY=1 -DCL_OPTIMIZATIONS=\"\"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmul.h"

#include "../../utils/time_ms.h"
#include "../../utils/cl_utils.h"

#define KERNEL_FILE_NAME __FILE__"l"

// structure for keeping track of opencl management data
typedef struct {
    cl_context ctx;
    cl_command_queue queue;
    cl_device_id id;
    cl_program prog;
    cl_kernel kernel;
} ocl_mgmt;

ocl_mgmt cl;

// opencl parameters - defined in main, used later in GL-main loop
typedef struct {
    size_t globalWorkGroupSize[2];
    size_t localWorkGroupSize[2];
    cl_mem A;
    cl_mem B;
    cl_mem C;
    cl_mem size;
} ocl_parameters;

ocl_parameters cl_param;



void multiplyStupidCL(REAL *A, REAL *B, REAL *C) {
	// initialize ocl device
	cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
	cl_int err;

	// create kernels from source
	char tmp[1024];
	sprintf(tmp, "%s -DREAL=%s -DN=%i -DTILESIZE=%i", CL_OPTIMIZATIONS, REAL_STRING, N, TILESIZE); // TILESIZE unused for naive matrix multiplication

	cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
	cl.kernel = clCreateKernel(cl.prog, "matrix_mulStupid", &err);
	CLU_ERRCHECK(err, "could not create kernel");

	cl_param.globalWorkGroupSize[0] = cl_param.globalWorkGroupSize[1] = N;

	// measuring time for calculation only, no prerequisites
	unsigned long long start_time = time_ms();
	// create buffers
	cl_param.A = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");
	cl_param.B = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");
	cl_param.C = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");

	// write buffers
	err = clEnqueueWriteBuffer(cl.queue, cl_param.A, CL_FALSE, 0, N*N*SIZEOF_CL_REAL, A, 0, NULL, NULL);
	err |= clEnqueueWriteBuffer(cl.queue, cl_param.B, CL_FALSE, 0, N*N*SIZEOF_CL_REAL, B, 0, NULL, NULL);
	CLU_ERRCHECK(err, "Failed to write buffers");

	// prepare kernel
	cluSetKernelArguments(cl.kernel, 3, sizeof(cl_mem), (void*)&cl_param.A, sizeof(cl_mem), (void*)&cl_param.B, sizeof(cl_mem), (void*)&cl_param.C);
	CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.kernel, 2, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue scan kernel");

	// readback data
	CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.C, CL_TRUE, 0, N*N*SIZEOF_REAL, C, 0, NULL, NULL), "Failed to read new positions");

	unsigned long long duration = time_ms() - start_time;
	printf("  elapsed time for multi-threaded GPU matrix multiplication: \t%6llu ms \n", duration);
	writeCSVllu(duration);


	// finalization
	err = clFinish(cl.queue);
	err |= clReleaseKernel(cl.kernel);
	err |= clReleaseProgram(cl.prog);
	err |= clReleaseCommandQueue(cl.queue);
	err |= clReleaseContext(cl.ctx);

	err |= clReleaseMemObject(cl_param.A);
	err |= clReleaseMemObject(cl_param.B);
	err |= clReleaseMemObject(cl_param.C);
	CLU_ERRCHECK(err, "Failed during ocl cleanup");
}

void multiplyNaiveCL(REAL *A, REAL *B, REAL *C) {
	// initialize ocl device
	cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
	cl_int err;

	// create kernels from source
	char tmp[1024];
	sprintf(tmp, "%s -DREAL=%s -DN=%i -DTILESIZE=%i", CL_OPTIMIZATIONS, REAL_STRING, N, TILESIZE); // TILESIZE unused for naive matrix multiplication

	cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
	cl.kernel = clCreateKernel(cl.prog, "matrix_mulNaive", &err);
	CLU_ERRCHECK(err, "could not create kernel");

	cl_param.globalWorkGroupSize[0] = cl_param.globalWorkGroupSize[1] = N;

	// measuring time for calculation only, no prerequisites
	unsigned long long start_time = time_ms();
	// create buffers
	cl_param.A = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");
	cl_param.B = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");
	cl_param.C = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");

	// write buffers
	err = clEnqueueWriteBuffer(cl.queue, cl_param.A, CL_FALSE, 0, N*N*SIZEOF_CL_REAL, A, 0, NULL, NULL);
	err |= clEnqueueWriteBuffer(cl.queue, cl_param.B, CL_FALSE, 0, N*N*SIZEOF_CL_REAL, B, 0, NULL, NULL);
	CLU_ERRCHECK(err, "Failed to write buffers");

	// prepare kernel
	cluSetKernelArguments(cl.kernel, 3, sizeof(cl_mem), (void*)&cl_param.A, sizeof(cl_mem), (void*)&cl_param.B, sizeof(cl_mem),(void*)&cl_param.C);
	CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.kernel, 2, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue scan kernel");

	// readback data
	CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.C, CL_TRUE, 0, N*N*SIZEOF_REAL, C, 0, NULL, NULL), "Failed to read new positions");

	unsigned long long duration = time_ms() - start_time;
	printf("  elapsed time for multi-threaded GPU matrix multiplication: \t%6llu ms \n", duration);
	writeCSVllu(duration);


	// finalization
	err = clFinish(cl.queue);
	err |= clReleaseKernel(cl.kernel);
	err |= clReleaseProgram(cl.prog);
	err |= clReleaseCommandQueue(cl.queue);
	err |= clReleaseContext(cl.ctx);

	err |= clReleaseMemObject(cl_param.A);
	err |= clReleaseMemObject(cl_param.B);
	err |= clReleaseMemObject(cl_param.C);
	CLU_ERRCHECK(err, "Failed during ocl cleanup");
}

void multiplyTilingCL(REAL *A, REAL *B, REAL *C) {
    // initialize ocl device
    cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
    cl_int err;

    // create kernels from source
    char tmp[1024];
    sprintf(tmp, "%s -DREAL=%s -DN=%i -DTILESIZE=%i", CL_OPTIMIZATIONS, REAL_STRING, N, TILESIZE);

    cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
    cl.kernel = clCreateKernel(cl.prog, "matrix_mulTiling", &err);
    CLU_ERRCHECK(err, "could not create kernel");

    cl_param.globalWorkGroupSize[0] = cl_param.globalWorkGroupSize[1] = N;
    cl_param.localWorkGroupSize[0] = cl_param.localWorkGroupSize[1] = TILESIZE;

    // measuring time for calculation only, no prerequisites
    unsigned long long start_time = time_ms();
    // create buffers
    cl_param.A = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");
    cl_param.B = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");
    cl_param.C = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, N*N*SIZEOF_CL_REAL, NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");

    // write buffers
    err = clEnqueueWriteBuffer(cl.queue, cl_param.A, CL_FALSE, 0, N*N*SIZEOF_CL_REAL, A, 0, NULL, NULL);
    err |= clEnqueueWriteBuffer(cl.queue, cl_param.B, CL_TRUE, 0, N*N*SIZEOF_CL_REAL, B, 0, NULL, NULL);
    CLU_ERRCHECK(err, "Failed to write buffers");

    // prepare kernel
    cluSetKernelArguments(cl.kernel, 3, sizeof(cl_mem), (void*)&cl_param.A, sizeof(cl_mem), (void*)&cl_param.B, sizeof(cl_mem), (void*)&cl_param.C);
    CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.kernel, 2, NULL, cl_param.globalWorkGroupSize, cl_param.localWorkGroupSize, 0, NULL, NULL), "Failed to enqueue scan kernel");

    // readback data
    CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.C, CL_TRUE, 0, N*N*SIZEOF_REAL, C, 0, NULL, NULL), "Failed to read new positions");

    unsigned long long duration = time_ms()-start_time;
    printf("  elapsed time for multi-threaded GPU matrix multiplication: \t%6llu ms \n", duration);
    writeCSVllu(duration);


    // finalization
    err = clFinish(cl.queue);
    err |= clReleaseKernel(cl.kernel);
    err |= clReleaseProgram(cl.prog);
    err |= clReleaseCommandQueue(cl.queue);
    err |= clReleaseContext(cl.ctx);

    err |= clReleaseMemObject(cl_param.A);
    err |= clReleaseMemObject(cl_param.B);
    err |= clReleaseMemObject(cl_param.C);
    CLU_ERRCHECK(err, "Failed during ocl cleanup");
}

int main(int argc, char** argv) {
    unsigned long long start_time = time_ms();
    if (argc == 2) {
        N = atoi(argv[1]);
        if (N % WORKGROUPSIZE != 0)
            N += WORKGROUPSIZE - (N % WORKGROUPSIZE);
        if (N <= 0)
            N = WORKGROUPSIZE;
    }
    if (N % TILESIZE != 0 || TILESIZE >= N) {
        printf("ERROR: tilesize (%d) has to be an integer divisor of N(%d).\n", TILESIZE, N);
        return EXIT_FAILURE;
    }
    if (TILESIZE*TILESIZE > 1024) {
        printf("ERROR: tilesize*tilesize (%d*%d) may not exceed 1024.\n", TILESIZE, TILESIZE);
        return EXIT_FAILURE;
    }
    initLogging(__FILE__, argv[0], "executable,number_type,N_dimension,WORKGROUPSIZE,TILESIZE,stupid_first_init_ms,stupid_second_ms,naive_first_init_ms,naive_second_ms,tiling_init_first_ms,tiling_second_ms,total_ms");
    writeCSV(REAL_STRING);
    writeCSVllu((unsigned long long) N);
    writeCSVllu(WORKGROUPSIZE);
    writeCSVllu(TILESIZE);

    printf("Matrix multiplication, using %dx%d matrice (%s), %s\n", N, N, REAL_STRING, strcmp(CL_OPTIMIZATIONS, "")==0?"with optimizations (-O3)":"without optimizations (-O0)");
    printf("  (adjusted to be a multiple of Workgroupsize %d)\n", WORKGROUPSIZE);
    printf("Tilesize: %d\n\n", TILESIZE);
	cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
	printf("%s\n\n", cluPrintDeviceAndVendor(cl.id));


    REAL *A = malloc(SIZEOF_REAL*N*N);
    REAL *B = malloc(SIZEOF_REAL*N*N);
    REAL *C = malloc(SIZEOF_REAL*N*N);

	printf("Stupid-naive matrix-multiplication:\n");
	initMatrices(A, B);

	printf("  first run - init\n");
	multiplyStupidCL(A, B, C);
	testResults(A, C);
	printf("\n  second run\n");
	multiplyStupidCL(A, B, C);

	printf("Naive matrix-multiplication:\n");
	printf("  first run - init\n");
	multiplyNaiveCL(A, B, C);
	testResults(A, C);
	printf("\n  second run\n");
	multiplyNaiveCL(A, B, C);

	printf("\nMatrix-multiplication with tiling:\n");
    printf("  first run - init\n");
    multiplyTilingCL(A, B, C);
    testResults(A, C);
    printf("\n  second run\n");
    multiplyTilingCL(A, B, C);

    free(A);
    free(B);
    free(C);
    unsigned long long duration = time_ms()-start_time;
    printf("total runtime: \t%6llu ms \n", duration);
    writeCSVllu(duration);
    writeCSV("\n");
    return EXIT_SUCCESS;
}
