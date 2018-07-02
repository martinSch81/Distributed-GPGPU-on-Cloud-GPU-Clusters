#if __OPENCL_VERSION__ <= CL_VERSION_1_1
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

__kernel void leibniz(__global REAL *subSums) {
    unsigned int gid = get_global_id(0);

    unsigned long n = gid*WORKSIZE;

    REAL subResult = 0.0;
    for (int part = WORKSIZE-1; part >= 0; part--) {
        // subResult += pow((REAL)-1, n+part) / (2*(n+part) + 1);
        if ((n + part) % 2 == 1)
            subResult += (REAL)-1 / ((2 * (n + part)) + 1);
        else
            subResult += (REAL)1 / ((2 * (n + part)) + 1);
    }
    subSums[gid] = subResult;
}

__kernel void reduction(__global REAL* subSums, __local REAL* localSums, __global REAL* results)
{
    unsigned int gid = get_global_id(0);
    unsigned int lid = get_local_id(0);
    int group_size = get_local_size(0);

    localSums[lid] = subSums[gid];
    barrier(CLK_LOCAL_MEM_FENCE);

    for (int i = group_size / 2; i>0; i /= 2) {
        if (lid < i)
            localSums[lid] += localSums[lid + i];

        barrier(CLK_LOCAL_MEM_FENCE);
    }

    if (lid == 0)
        results[get_group_id(0)] = localSums[0];
}

