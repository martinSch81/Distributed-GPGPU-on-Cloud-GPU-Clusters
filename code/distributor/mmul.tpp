// Matrix multiplication
// author: Schuchardt Martin, csap9442

template<typename T>
void initMatrices(T *A, T *B, const unsigned N) {
	for (unsigned i = 0; i < N; i++) {
		for (unsigned j = 0; j < N; j++) {
			*(A + i*N + j) = rand()%100;
			// *(A + i*N + j) = i*N + j;
			*(B + i*N + j) = (i == j) ? 1 : 0; // identity matrix
		}
	}
}

template<typename T>
int testResults(T *should, T *is, const std::size_t N, std::string &output) {
	if (!VERIFY) return 0;
	output = "...verifying...\n";
	for (unsigned i = 0; i < N; i++)
		for (unsigned j = 0; j < N; j++) {
			if (abs((should[i*N + j]) - is[i*N + j]) > EPSILON) {
				output += "ERROR: values do not match: should[" + std::to_string(i) + ", " + std::to_string(j) + "]: " + std::to_string(*(should + i*N + j)) + ", is[" + std::to_string(i) + ", " + std::to_string(j) + "]: " + std::to_string(*(is + i*N + j)) + '\n';
				return 1;
			}
		}

	output += "...done!\n";
	return 0;
}

template<typename T>
void printMatrix(T *A, unsigned I, unsigned J) {
	for (unsigned i = 0; i < I; i++) {
		for (unsigned j = 0; j < J; j++) {
			std::cout << *(A + i*I + j) << '\t';
		}
		std::cout << std::endl;
	}
}


// *** MMul/OpenCL code **********************************************************************************************************************************
// simple matrix multiplication using CPU
void multiplyChunkCPU(const DATA_TYPE *A, const int ROWS, const int COLUMNS, const DATA_TYPE *B, DATA_TYPE *C) {
	for (int i = 0; i < ROWS; ++i) {
		for (int j = 0; j < COLUMNS; ++j) {
			DATA_TYPE sum = 0;
			for (int k = 0; k < COLUMNS; ++k) {
				sum += *(A + i*COLUMNS + k) * *(B + k*COLUMNS + j);
			}
			*(C + i*COLUMNS + j) = sum;
		}
	}
}

// simple matrix multiplication using openCL
// ATTENTION: for integer-matrices only!
void multiplyChunkCL(const DATA_TYPE_CL *A, const int ROWS, const int COLUMNS, const DATA_TYPE_CL *B, DATA_TYPE_CL *C, unsigned acc_device) {
	// initialize ocl device
	cl.id = cluInitDevice(acc_device, &cl.ctx, &cl.queue);
	int err = 0;

	// create kernels from source
	char tmp[1024];
	sprintf(tmp, "-DN=%i -DDATA_TYPE=%s", COLUMNS, DATA_TYPE_STRING);

	// reading kernel from string spares erroneous mpi --preload-files
	// const std::string KERNEL_FILE_NAME = getDirectory(__FILE__) + "/mmul.cl";
	// cl.prog = cluBuildProgramFromFile(cl.ctx, cl.id, KERNEL_FILE_NAME.c_str(), tmp);
	cl.prog = cluBuildProgramFromString(cl.ctx, cl.id, kernelCode.c_str(), tmp);
	cl.kernel = clCreateKernel(cl.prog, "mmulNaive", &err);
	CLU_ERRCHECK(err, "could not create kernel");

	cl_param.globalWorkGroupSize[0] = COLUMNS;
	cl_param.globalWorkGroupSize[1] = ROWS;

	// create buffers
	cl_param.A = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, COLUMNS*ROWS * sizeof(DATA_TYPE_CL), NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");
	cl_param.B = clCreateBuffer(cl.ctx, CL_MEM_READ_ONLY, COLUMNS*COLUMNS * sizeof(DATA_TYPE_CL), NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");
	cl_param.C = clCreateBuffer(cl.ctx, CL_MEM_WRITE_ONLY, COLUMNS*ROWS * sizeof(DATA_TYPE_CL), NULL, &err);
	CLU_ERRCHECK(err, "Failed to create buffer");

	// write buffers
	err = clEnqueueWriteBuffer(cl.queue, cl_param.A, CL_FALSE, 0, COLUMNS*ROWS * sizeof(DATA_TYPE_CL), A, 0, NULL, NULL);
	err |= clEnqueueWriteBuffer(cl.queue, cl_param.B, CL_FALSE, 0, COLUMNS*COLUMNS * sizeof(DATA_TYPE_CL), B, 0, NULL, NULL);
	CLU_ERRCHECK(err, "Failed to write buffers");

	// prepare kernel
	cluSetKernelArguments(cl.kernel, 3, sizeof(cl_mem), (void*)&cl_param.A, sizeof(cl_mem), (void*)&cl_param.B, sizeof(cl_mem), (void*)&cl_param.C);
	CLU_ERRCHECK(clEnqueueNDRangeKernel(cl.queue, cl.kernel, 2, NULL, cl_param.globalWorkGroupSize, NULL, 0, NULL, NULL), "Failed to enqueue scan kernel");

	// readback data
	CLU_ERRCHECK(clEnqueueReadBuffer(cl.queue, cl_param.C, CL_TRUE, 0, COLUMNS*ROWS * sizeof(DATA_TYPE_CL), C, 0, NULL, NULL), "Failed to read new positions");


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
// *** MMul/OpenCL code **********************************************************************************************************************************
