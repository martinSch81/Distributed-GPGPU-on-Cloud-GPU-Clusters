// Device Info using OpenCL methods. List all found and supported devices with details.
// author: Schuchardt Martin, csap9442
// compile: gcc -O0 -std=c99 -Wall -Werror showDevices.c -I/usr/include -L/usr/lib -lOpenCL -o showDevices

// #define _CRT_SECURE_NO_DEPRECATE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include "../utils/cl_utils.h"

#ifdef __linux
#include <unistd.h>
#define INIT_HOSTNAME_FOR_WINDOWS {}
#else
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#define INIT_HOSTNAME_FOR_WINDOWS {WORD wVersionRequested = MAKEWORD(2, 2); WSADATA wsaData; WSAStartup(wVersionRequested, &wsaData);}
#endif

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

void listPlatform(cl_platform_id platform_id) {
    char device_string[1024];

	clGetPlatformInfo(platform_id, CL_PLATFORM_NAME, sizeof(device_string), &device_string, NULL);
	printf("  CL_PLATFORM_NAME: \t\t\t%s\n", device_string);

	clGetPlatformInfo(platform_id, CL_PLATFORM_PROFILE, sizeof(device_string), &device_string, NULL);
	printf("  CL_PLATFORM_PROFILE: \t\t\t%s\n", device_string);

	clGetPlatformInfo(platform_id, CL_PLATFORM_VENDOR, sizeof(device_string), &device_string, NULL);
	printf("  CL_PLATFORM_VENDOR: \t\t\t%s\n", device_string);

	clGetPlatformInfo(platform_id, CL_PLATFORM_VERSION, sizeof(device_string), &device_string, NULL);
	printf("  CL_PLATFORM_VERSION: \t\t\t%s\n", device_string);

	clGetPlatformInfo(platform_id, CL_PLATFORM_EXTENSIONS, sizeof(device_string), &device_string, NULL);
	printf("  CL_PLATFORM_EXTENSIONS: \t\t%s\n", device_string);
}

void listAccelerator(cl_device_id device_id) {
	char device_string[1024];

	clGetDeviceInfo(device_id, CL_DEVICE_NAME, sizeof(device_string), &device_string, NULL);
	printf("  CL_DEVICE_NAME: \t\t\t%s\n", trim(device_string));

	clGetDeviceInfo(device_id, CL_DEVICE_VENDOR, sizeof(device_string), &device_string, NULL);
	printf("  CL_DEVICE_VENDOR: \t\t\t%s\n", trim(device_string));

	clGetDeviceInfo(device_id, CL_DRIVER_VERSION, sizeof(device_string), &device_string, NULL);
	printf("  CL_DRIVER_VERSION: \t\t\t%s\n", trim(device_string));

	clGetDeviceInfo(device_id, CL_DEVICE_EXTENSIONS, sizeof(device_string), &device_string, NULL);
	printf("  CL_DEVICE_EXTENSIONS: \t\t%s\n\n", trim(device_string));

	cl_device_type type;
	clGetDeviceInfo(device_id, CL_DEVICE_TYPE, sizeof(type), &type, NULL);
	if (type & CL_DEVICE_TYPE_CPU)
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_CPU");
	if (type & CL_DEVICE_TYPE_GPU)
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_GPU");
	if (type & CL_DEVICE_TYPE_ACCELERATOR)
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_ACCELERATOR");
	if (type & CL_DEVICE_TYPE_DEFAULT)
		printf("  CL_DEVICE_TYPE:\t\t\t%s\n", "CL_DEVICE_TYPE_DEFAULT");

	cl_uint compute_units;
	clGetDeviceInfo(device_id, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(compute_units), &compute_units, NULL);
	printf("  CL_DEVICE_MAX_COMPUTE_UNITS:\t\t%u\n", compute_units);

	size_t workitem_dims;
	clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(workitem_dims), &workitem_dims, NULL);
	printf("  CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:\t%d\n", (int)workitem_dims);

	size_t workitem_size[3];
	clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(workitem_size), &workitem_size, NULL);
	printf("  CL_DEVICE_MAX_WORK_ITEM_SIZES:\t%u / %u / %u \n", (int)workitem_size[0], (int)workitem_size[1], (int)workitem_size[2]);

	size_t workgroup_size;
	clGetDeviceInfo(device_id, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(workgroup_size), &workgroup_size, NULL);
	printf("  CL_DEVICE_MAX_WORK_GROUP_SIZE:\t%u\n", (int)workgroup_size);

	cl_uint clock_frequency;
	clGetDeviceInfo(device_id, CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(clock_frequency), &clock_frequency, NULL);
	printf("  CL_DEVICE_MAX_CLOCK_FREQUENCY:\t%u MHz\n", clock_frequency);

	cl_uint addr_bits;
	clGetDeviceInfo(device_id, CL_DEVICE_ADDRESS_BITS, sizeof(addr_bits), &addr_bits, NULL);
	printf("  CL_DEVICE_ADDRESS_BITS:\t\t%u\n", addr_bits);

	cl_ulong max_mem_alloc_size;
	clGetDeviceInfo(device_id, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(max_mem_alloc_size), &max_mem_alloc_size, NULL);
	printf("  CL_DEVICE_MAX_MEM_ALLOC_SIZE:\t\t%u MByte\n", (unsigned int)(max_mem_alloc_size / (1024 * 1024)));

	cl_ulong mem_size;
	clGetDeviceInfo(device_id, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
	printf("  CL_DEVICE_GLOBAL_MEM_SIZE:\t\t%u MByte\n", (unsigned int)(mem_size / (1024 * 1024)));

	cl_bool error_correction_support;
	clGetDeviceInfo(device_id, CL_DEVICE_ERROR_CORRECTION_SUPPORT, sizeof(error_correction_support), &error_correction_support, NULL);
	printf("  CL_DEVICE_ERROR_CORRECTION_SUPPORT:\t%s\n", error_correction_support == CL_TRUE ? "yes" : "no");

	cl_device_local_mem_type local_mem_type;
	clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_TYPE, sizeof(local_mem_type), &local_mem_type, NULL);
	printf("  CL_DEVICE_LOCAL_MEM_TYPE:\t\t%s\n", local_mem_type == 1 ? "local" : "global");

	clGetDeviceInfo(device_id, CL_DEVICE_LOCAL_MEM_SIZE, sizeof(mem_size), &mem_size, NULL);
	printf("  CL_DEVICE_LOCAL_MEM_SIZE:\t\t%u KByte\n", (unsigned int)(mem_size / 1024));

	clGetDeviceInfo(device_id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, sizeof(mem_size), &mem_size, NULL);
	printf("  CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:\t%u KByte\n", (unsigned int)(mem_size / 1024));
}

void listAllAccelerators() {
    // get platform ids
    cl_uint ret_num_platforms;
    clGetPlatformIDs(0, NULL, &ret_num_platforms);
    cl_platform_id *ret_platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id)*ret_num_platforms);
    clGetPlatformIDs(ret_num_platforms, ret_platforms, NULL);

	unsigned acc_device = 0;
    // get device id of desired device
    for (cl_uint i = 0; i < ret_num_platforms; ++i) {
        cl_uint ret_num_devices;
        clGetDeviceIDs(ret_platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &ret_num_devices);
		printf("********************************************************************************\n");
		printf("*** platform_id[%d] *************************************************************\n", i);
		printf("********************************************************************************\n");
		listPlatform(ret_platforms[i]);
		printf("\n");
		cl_device_id *ret_devices = (cl_device_id*)malloc(sizeof(cl_device_id)*ret_num_devices);
        clGetDeviceIDs(ret_platforms[i], CL_DEVICE_TYPE_ALL, ret_num_devices, ret_devices, NULL);
        for (cl_uint j = 0; j < ret_num_devices; j++) {
			printf("*** platform_id[%d], device_id[%d], ACC_DEVICE=%d *********************************\n", i, j, acc_device++);
            listAccelerator(ret_devices[j]);
			printf("********************************************************************************\n");
        }
        free(ret_devices);
		printf("********************************************************************************\n");
		printf("\n");
	}

    free(ret_platforms);
}

int main() {
	INIT_HOSTNAME_FOR_WINDOWS
	char hostname[128];
    gethostname(hostname, 128);
    printf("Hostname: %s\n\n", hostname);
    listAllAccelerators();
    return EXIT_SUCCESS;
}
