// Calculation of PI using Leibniz formula for OpenCL
// author: Schuchardt Martin, csap9442
// compile: gcc -O3 -std=c99 -Wall -Werror -lm leibniz.c leibnizCL.c -lOpenCL -o leibnizCL_D2_double -DNUMBER_TYPE=2 -DWORKSIZE=32768 -DACC_DEVICE=2 -DCL_OPTIMIZATIONS=\"\"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "leibniz.h"

#include "../../utils/time_ms.h"
#include "../../utils/cl_utils.h"

#define KERNEL_FILE_NAME __FILE__"l"

// structure for keeping track of opencl management data
typedef struct {
    cl_context ctx;
    cl_command_queue queue;
    cl_device_id id;
    cl_program prog;
    cl_kernel kernel_leibniz;
    cl_kernel kernel_reduction;
} ocl_mgmt;

ocl_mgmt cl;

// opencl parameters
typedef struct {
    size_t globalWorkGroupSize[1];
    size_t localWorkGroupSize[1];
    cl_mem subSums;
    cl_mem results;
    int subSumsSize;
} ocl_parameters;

ocl_parameters cl_param;


REAL leibnizCL(unsigned long long STEPS) {
    int NBRTHREADS = STEPS / WORKSIZE;

    // initialize ocl device
    cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
    cl_int err;

    // create kernels from source
    char tmp[1024];
    sprintf(tmp, "%s -DREAL=%s -DWORKSIZE=%d", CL_OPTIMIZATIONS, REAL_STRING, WORKSIZE);
    cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME, tmp);
    cl.kernel_leibniz = clCreateKernel(cl.prog, "leibniz", &err);
    CLU_ERRCHECK(err, "could not create kernel");
    cl.kernel_reduction = clCreateKernel(cl.prog, "reduction", &err);
    CLU_ERRCHECK(err, "could not create kernel");

    cl_param.globalWorkGroupSize[0] = NBRTHREADS;
    cl_param.localWorkGroupSize[0] = REDUCTION_WORKGROUP;

    int nbrResults = NBRTHREADS / REDUCTION_WORKGROUP;
    if (NBRTHREADS % REDUCTION_WORKGROUP != 0)
        nbrResults++;

    unsigned long long start_time = time_ms();
    // create buffers
    cl_param.subSumsSize = NBRTHREADS*SIZEOF_CL_REAL;
    cl_param.subSums = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, cl_param.subSumsSize, NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");
    cl_param.results = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, nbrResults*SIZEOF_CL_REAL, NULL, &err);
    CLU_ERRCHECK(err, "Failed to create buffer");

    // write buffers
    // nothing to do here :)

    // prepare kernel
    cluSetKernelArguments(cl.kernel_leibniz, 1, sizeof(cl_mem), (void*)&cl_param.subSums);
    CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.kernel_leibniz, 1, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue scan kernel");

    cluSetKernelArguments(cl.kernel_reduction, 3, sizeof(cl_mem), (void*)&cl_param.subSums, cl_param.localWorkGroupSize[0]*SIZEOF_CL_REAL, NULL, sizeof(cl_mem), (void*)&cl_param.results);
    CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.kernel_reduction, 1, NULL, cl_param.globalWorkGroupSize, cl_param.localWorkGroupSize, 0, NULL, NULL), "Failed to enqueue scan kernel");
    REAL *results = malloc(SIZEOF_REAL*nbrResults);
    CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.results, CL_TRUE, 0, nbrResults*SIZEOF_CL_REAL, results, 0, NULL, NULL), "Failed to read new positions");

    unsigned long long duration = time_ms()-start_time;
    printf("  elapsed time for multi-threaded GPU leibniz-pi-calculation: \t%6llu ms \n", duration);
    writeCSVllu(duration);

    // finalization
    err = clFinish(cl.queue);
    err |= clReleaseKernel(cl.kernel_leibniz);
    err |= clReleaseKernel(cl.kernel_reduction);
    err |= clReleaseProgram(cl.prog);
    err |= clReleaseCommandQueue(cl.queue);
    err |= clReleaseContext(cl.ctx);

    err |= clReleaseMemObject(cl_param.subSums);
    err |= clReleaseMemObject(cl_param.results);
    CLU_ERRCHECK(err, "Failed during ocl cleanup");

    // reduce rest with CPU
    REAL result = 0.0;
    for (int grp = nbrResults - 1; grp >= 0; grp--) {
        result += results[grp];
    }
    free(results);

    return result * 4;
}

int main(int argc, char** argv) {
    unsigned long long start_time = time_ms();
    if (argc == 2) {
        STEPS = atol(argv[1]);
        if (STEPS % WORKSIZE != 0)
            STEPS += WORKSIZE - (STEPS % WORKSIZE);
        if (STEPS <= 0)
            STEPS = WORKSIZE;

        if (STEPS/WORKSIZE > INT_MAX/2) {
            printf("worksize too small: %d\n", WORKSIZE);
            return EXIT_FAILURE;
        }

        printf("Leibniz forumula, using userspecified number of steps: %llu, data type %s, %s\n", STEPS, REAL_STRING, strcmp(CL_OPTIMIZATIONS, "") == 0 ? "with optimizations (-O3)" : "without optimizations (-O0)");
        printf("  (adjusted to be a multiple of Worksize %d)\n", WORKSIZE);
    } else {
        printf("Leibniz formula, using default number of steps: %llu, data type %s, %s\n", STEPS, REAL_STRING, strcmp(CL_OPTIMIZATIONS, "") == 0 ? "with optimizations (-O3)" : "without optimizations (-O0)");
    }
    initLogging(__FILE__, argv[0], "executable,number_type,STEPS,WORKSIZE,first_init_ms,second_ms,total_ms");
    writeCSV(REAL_STRING);
    writeCSVllu(STEPS);
    writeCSVllu(WORKSIZE);

	cl.id = cluInitDevice(ACC_DEVICE, &cl.ctx, &cl.queue);
	printf("\n%s\n\n", cluPrintDeviceAndVendor(cl.id));


    printf("first run - init\n");
    REAL result = leibnizCL(STEPS);
    verifyPI(result);
    printf("second run\n");
    result = leibnizCL(STEPS);

    unsigned long long duration = time_ms()-start_time;
    printf("total runtime: \t%6llu ms \n", duration);
    writeCSVllu(duration);
    writeCSV("\n");
    return EXIT_SUCCESS;
}
