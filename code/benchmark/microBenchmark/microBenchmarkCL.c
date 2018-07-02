// Micro Benchmark using
//   int: +, -, *, /, %
//   float: +, -, *, /, sin, tan, cos, log, sqrt, pow
//   double: +, -, *, /, sin, tan, cos, log, sqrt, pow
// author: Schuchardt Martin, csap9442
// compile: gcc -O0 -std=c99 -Wall -Werror -lm microBenchmarkCL.c microBenchmark.o -lOpenCL -o microBenchmarkCL_D1 -DACC_DEVICE=1 -DWORKGROUPSIZE=64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "microBenchmark.h"

#include "../../utils/time_ms.h"
#include "../../utils/cl_utils.h"

#define KERNEL_FILE_NAME __FILE__"l"

// structure for keeping track of opencl management data
typedef struct {
    cl_context ctx;
    cl_command_queue queue;
    cl_device_id id;
    cl_program prog;
    cl_kernel intKernel;
    cl_kernel floatKernel;
    cl_kernel doubleKernel;
} ocl_mgmt;

ocl_mgmt cl;

// opencl parameters
typedef struct {
    size_t globalWorkGroupSize[1];
    //size_t localWorkGroupSize[1];
    cl_mem intResults;
    cl_mem floatResults;
    cl_mem doubleResults;
    cl_mem iters;
} ocl_parameters;

ocl_parameters cl_param;


int* microBenchmarkIntCL() {
    int *result = malloc(sizeof(int)*THREADS);

    // initialize ocl device
    cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
    cl_int err;

    // create kernels from source
	char tmp[1024];
	sprintf(tmp, "%s", CL_OPTIMIZATIONS);

    cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
    cl.intKernel = clCreateKernel(cl.prog, "microBenchInt", &err);
    CLU_ERRCHECK(err, "could not create kernel");

    cl_param.globalWorkGroupSize[0] = THREADS;

    unsigned long long start_time = time_ms();
    // create buffers
    cl_param.intResults = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, sizeof(cl_int)*THREADS, NULL, &err);
    cl_param.iters = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, sizeof(cl_int), NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");

    // write buffers
    err = clEnqueueWriteBuffer(cl.queue, cl_param.iters, CL_TRUE, 0, sizeof(cl_int), &ITERS, 0, NULL, NULL);
    CLU_ERRCHECK(err, "Failed to write arguments to device");

    // prepare kernel
    cluSetKernelArguments(cl.intKernel, 2, sizeof(cl_mem), (void *)&cl_param.iters, sizeof(cl_mem), (void*)&cl_param.intResults);

    CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.intKernel, 1, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue kernel");
    CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.intResults, CL_TRUE, 0, THREADS*sizeof(int), result, 0, NULL, NULL), "Failed to read results");

    unsigned long long duration = time_ms()-start_time;
    printf("  elapsed time for multi-threaded GPU int arithmetic microbenchmark: \t%6llu ms \n", duration);
    writeCSVllu(duration);


    // finalization
    err = clFinish(cl.queue);
    err |= clReleaseKernel(cl.intKernel);
    err |= clReleaseProgram(cl.prog);
    err |= clReleaseCommandQueue(cl.queue);
    err |= clReleaseContext(cl.ctx);

    err |= clReleaseMemObject(cl_param.intResults);
    err |= clReleaseMemObject(cl_param.iters);
    CLU_ERRCHECK(err, "Failed during ocl cleanup");

    return result;
}

float* microBenchmarkFloatCL() {
    float *result = malloc(sizeof(float)*THREADS);

    // initialize ocl device
    cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
    cl_int err;

    // create kernels from source
	char tmp[1024];
	sprintf(tmp, "%s", CL_OPTIMIZATIONS);

	cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
    cl.floatKernel = clCreateKernel(cl.prog, "microBenchFloat", &err);
    CLU_ERRCHECK(err, "could not create kernel");

    cl_param.globalWorkGroupSize[0] = THREADS;

    unsigned long long start_time = time_ms();
    // create buffers
    cl_param.floatResults = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, sizeof(cl_float)*THREADS, NULL, &err);
    cl_param.iters = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, sizeof(cl_int), NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");

    // write buffers
    err = clEnqueueWriteBuffer(cl.queue, cl_param.iters, CL_TRUE, 0, sizeof(cl_int), &ITERS, 0, NULL, NULL);
    CLU_ERRCHECK(err, "Failed to write arguments to device");

    // prepare kernel
    cluSetKernelArguments(cl.floatKernel, 2, sizeof(cl_mem), (void *)&cl_param.iters, sizeof(cl_mem), (void*)&cl_param.floatResults);

    CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.floatKernel, 1, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue kernel");
    CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.floatResults, CL_TRUE, 0, THREADS*sizeof(float), result, 0, NULL, NULL), "Failed to read results");
    unsigned long long duration = time_ms()-start_time;
    printf("  elapsed time for multi-threaded GPU float arithmetic microbenchmark: \t%6llu ms \n", duration);
    writeCSVllu(duration);


    // finalization
    err = clFinish(cl.queue);
    err |= clReleaseKernel(cl.floatKernel);
    err |= clReleaseProgram(cl.prog);
    err |= clReleaseCommandQueue(cl.queue);
    err |= clReleaseContext(cl.ctx);

    err |= clReleaseMemObject(cl_param.floatResults);
    err |= clReleaseMemObject(cl_param.iters);
    CLU_ERRCHECK(err, "Failed during ocl cleanup");

    return result;
}

double* microBenchmarkDoubleCL() {
    double *result = malloc(sizeof(double)*THREADS);

    // initialize ocl device
    cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
    cl_int err;

    // create kernels from source
	char tmp[1024];
	sprintf(tmp, "%s", CL_OPTIMIZATIONS);

	cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
	cl.doubleKernel = clCreateKernel(cl.prog, "microBenchDouble", &err);
    CLU_ERRCHECK(err, "could not create kernel");

    cl_param.globalWorkGroupSize[0] = THREADS;

    unsigned long long start_time = time_ms();
    // create buffers
    cl_param.doubleResults = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, sizeof(cl_double)*THREADS, NULL, &err);
    cl_param.iters = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, sizeof(cl_int), NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");

    // write buffers
    err = clEnqueueWriteBuffer(cl.queue, cl_param.iters, CL_TRUE, 0, sizeof(cl_int), &ITERS, 0, NULL, NULL);
    CLU_ERRCHECK(err, "Failed to write arguments to device");

    // prepare kernel
    cluSetKernelArguments(cl.doubleKernel, 2, sizeof(cl_mem), (void *)&cl_param.iters, sizeof(cl_mem), (void*)&cl_param.doubleResults);

    CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.doubleKernel, 1, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue kernel");
    CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.doubleResults, CL_TRUE, 0, THREADS*sizeof(double), result, 0, NULL, NULL), "Failed to read results");
    unsigned long long duration = time_ms()-start_time;
    printf("  elapsed time for multi-threaded GPU double arithmetic microbenchmark: %6llu ms \n", duration);
    writeCSVllu(duration);


    // finalization
    err = clFinish(cl.queue);
    err |= clReleaseKernel(cl.doubleKernel);
    err |= clReleaseProgram(cl.prog);
    err |= clReleaseCommandQueue(cl.queue);
    err |= clReleaseContext(cl.ctx);

    err |= clReleaseMemObject(cl_param.doubleResults);
    err |= clReleaseMemObject(cl_param.iters);
    CLU_ERRCHECK(err, "Failed during ocl cleanup");

    return result;
}

int main(int argc, char** argv) {
    unsigned long long start_time = time_ms();
    printf("Micro Benchmark\n");
    if (argc == 3) {
        THREADS = atoi(argv[1]);
        if (THREADS % WORKGROUPSIZE != 0)
            THREADS += WORKGROUPSIZE - (THREADS % WORKGROUPSIZE);
        if (THREADS <= 0)
            THREADS = WORKGROUPSIZE;

        ITERS = atoi(argv[2]);

        printf("using userspecified number of threads: %d\n  (adjusted to be a multiple of Workgroupsize %d)\n", THREADS, WORKGROUPSIZE);
        printf("and iterations per thread: %d, %s\n\n", ITERS, strcmp(CL_OPTIMIZATIONS, "") == 0 ? "with optimizations (-O3)" : "without optimizations (-O0)");
    } else {
        printf("using default number of threads: %d and iterations per thread: %d, %s\n\n", THREADS, ITERS, strcmp(CL_OPTIMIZATIONS, "") == 0 ? "with optimizations (-O3)" : "without optimizations (-O0)");
    }
    initLogging(__FILE__, argv[0], "executable,THREADS,ITERS,WORKGROUPSIZE,first_init_int_ms,first_init_float_ms,first_init_double_ms,second_int_ms,second_float_ms,second_double_ms,total_ms");
    writeCSVllu(THREADS);
    writeCSVllu(ITERS);
    writeCSVllu(WORKGROUPSIZE);

	cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
	printf("\n%s\n\n", cluPrintDeviceAndVendor(cl.id));


    printf("first run - init\n");
    int* intResults = microBenchmarkIntCL();
    verifyInt(intResults);
    float* floatResults = microBenchmarkFloatCL();
    verifyFloat(floatResults);
    double* doubleResults = microBenchmarkDoubleCL();
    verifyDouble(doubleResults);
    free(intResults);
    free(floatResults);
    free(doubleResults);

    printf("\n\nsecond run\n");
    intResults = microBenchmarkIntCL();
    floatResults = microBenchmarkFloatCL();
    doubleResults = microBenchmarkDoubleCL();

    free(intResults);
    free(floatResults);
    free(doubleResults);

    unsigned long long duration = time_ms()-start_time;
    printf("total runtime: \t%6llu ms \n", duration);
    writeCSVllu(duration);
    writeCSV("\n");
    return EXIT_SUCCESS;
}
