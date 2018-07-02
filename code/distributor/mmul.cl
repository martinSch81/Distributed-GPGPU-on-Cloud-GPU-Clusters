// author: Schuchardt Martin, csap9442
std::string kernelCode =
"__kernel void mmulNaive(__global DATA_TYPE *A, __global DATA_TYPE *B, __global DATA_TYPE *C) {"
"	int col = get_global_id(0);																   "
"	int row = get_global_id(1);																   "
"	DATA_TYPE sum = 0;																		   "
"	for (int k = 0; k<N; ++k) {																   "
"		sum += *(A + row*N + k) * *(B + k*N + col);											   "
"	}																						   "
"	*(C + row*N + col) = sum;																   "
"}																						       ";