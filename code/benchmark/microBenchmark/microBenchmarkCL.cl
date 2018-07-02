#if __OPENCL_VERSION__ <= CL_VERSION_1_1
  #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

__kernel void microBenchInt(__global const int *ITERS, __global int *results) {
    unsigned int gid = get_global_id(0);

    int init = gid+1;
    int result = 0;
    for (int iter = 0; iter < *ITERS; iter++) {
        result += init;
        result *= init;
        result -= init;
        result /= init;
        result %= init;
    }

    results[gid] = result;
}

__kernel void microBenchFloat(__global const int *ITERS, __global float *results) {
    unsigned int gid = get_global_id(0);

    float init = gid+1;
    float result = 0.0f;
    for (int iter = 0; iter < *ITERS; iter++) {
        result += init;
        result *= init;
        result = sqrt(result);
        result = log(result);
        result -= init;
        result /= init;
        result = sin(result);
        result = tan(result);
        result = cos(result);
        result = pow(result, init);
    }

    results[gid] = result;
}

__kernel void microBenchDouble(__global const int *ITERS, __global double *results) {
    unsigned int gid = get_global_id(0);
    double init = gid+1;
    double result = 0.0;
    for (int iter = 0; iter < *ITERS; iter++) {
        result += init;
        result *= init;
        result = sqrt(result);
        result = log(result);
        result -= init;
        result /= init;
        result = sin(result);
        result = tan(result);
        result = cos(result);
        result = pow(result, init);
    }
    results[gid] = result;
}
