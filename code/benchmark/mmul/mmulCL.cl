#if __OPENCL_VERSION__ <= CL_VERSION_1_1
  #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

__kernel void matrix_mulStupid(__global REAL *A, __global REAL *B, __global REAL *C) {
	int row = get_global_id(0);
	int col = get_global_id(1);
	REAL sum = 0;
	for (int k = 0; k<N; ++k) {
		sum += *(A + row*N + k) * *(B + k*N + col);
	}
	*(C + row*N + col) = sum;
}

__kernel void matrix_mulNaive(__global REAL *A, __global REAL *B, __global REAL *C) {
	int col = get_global_id(0);
	int row = get_global_id(1);
	REAL sum = 0;
	for (int k = 0; k<N; ++k) {
		sum += *(A + row*N + k) * *(B + k*N + col);
	}
	*(C + row*N + col) = sum;
}

__kernel void matrix_mulTiling(__global REAL *A, __global REAL *B, __global REAL *C) {
	int gIdCol = get_global_id(0);
	int gIdRow = get_global_id(1);
	int lIdCol = get_local_id(0);
	int lIdRow = get_local_id(1);

	REAL tmp_sum = 0;

	for (int i = 0; i < N; i += TILESIZE) {
		__local REAL localA[TILESIZE][TILESIZE];
		__local REAL localB[TILESIZE][TILESIZE];

		localA[lIdRow][lIdCol] = A[gIdRow * N + i + lIdCol];
		localB[lIdRow][lIdCol] = B[(i + lIdRow) * N + gIdCol];

		barrier(CLK_LOCAL_MEM_FENCE);

		for (int k = 0; k < TILESIZE; k++) {
			tmp_sum += localA[lIdRow][k] * localB[k][lIdCol];
		}

		barrier(CLK_LOCAL_MEM_FENCE);
	}

	C[gIdRow * N + gIdCol] = tmp_sum;
}
