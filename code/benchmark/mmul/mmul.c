// Matrix multiplication.
// author: Schuchardt Martin, csap9442

#include "mmul.h"

int N = 1024;
float EPSILON = 0.00000000000000000001f;

void initMatrices(REAL *A, REAL *B) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            *(A + i*N + j) = rand();
            *(B + i*N + j) = (i == j) ? 1 : 0; // identity matrix
        }
    }
}

void zeroMatrix(REAL *C) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            *(C + i*N + j) = 0;
        }
    }
}

void testResults(REAL *should, REAL *is) {
    if (!VERIFY) return;
    printf("  ...verifying...");
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            if (fabs((should[i*N + j]) - is[i*N + j])>EPSILON) {
                printf("ERROR: values do not match: should[%d]: %20.52f, is[%d]: %20.52f\n", (i*N + j), (double)*(should + i*N + j), (i*N + j), (double)*(is + i*N + j));
                return;
            }

    printf("  ...done!\n");
}
