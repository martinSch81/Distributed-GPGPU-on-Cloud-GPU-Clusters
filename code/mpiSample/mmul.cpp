// Matrix multiplication.
// author: Schuchardt Martin, csap9442

#include "mmul.h"

int N = 1000;
float EPSILON = 0.00000000000000000001f;

void initMatrices(int *A, int *B) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            // *(A + i*N + j) = rand();
            *(A + i*N + j) = i*N + j;
            *(B + i*N + j) = (i == j) ? 1 : 0; // identity matrix
        }
    }
}

void testResults(int *should, int *is) {
    if (!VERIFY) return;
    std::cout << "  ...verifying..." << std::endl;
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (abs((should[i*N + j]) - is[i*N + j])!=0) {
				        std::cout << "ERROR: values do not match: should[" << i << ", " << j << "]: " << *(should + i*N + j) << ", is[" << i << ", " << j << "]: " << *(is + i*N + j) << std::endl;
                return;
            }

    std::cout << "  ...done!\n" << std::endl;
}

void printMatrix(int *A, int I, int J) {
    for (int i = 0; i < I; i++) {
        for (int j = 0; j < J; j++) {
			std::cout << *(A + i*I + j) << '\t';
        }
		std::cout << std::endl;
    }
}
