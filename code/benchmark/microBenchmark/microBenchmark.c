// Micro Benchmark using
//   int: +, -, *, /, %
//   float: +, -, *, /, sin, tan, cos, log, sqrt, pow
//   double: +, -, *, /, sin, tan, cos, log, sqrt, pow
// author: Schuchardt Martin, csap9442
// compile: gcc -O0 -std=c99 -Wall -Werror -lm microBenchmark.c -c

#include "microBenchmark.h"

int THREADS = 100000 / WORKGROUPSIZE*WORKGROUPSIZE;
int ITERS = 750;
float EPSILON = 0.000002f;
int NBR_RESULTS = 10000; // may not exceed THREADS/2

int* samplesInt() {
    int *results = malloc(sizeof(int)*THREADS);

    for (int thread = 0; thread < NBR_RESULTS; thread++) {
        int init = thread + 1;
        int result = 0;
        for (int iter = 0; iter < ITERS; iter++) {
            result += init;
            result *= init;
            result -= init;
            result /= init;
            result %= init;
        }

        results[thread] = result;
    }

    for (int thread = NBR_RESULTS; thread < (THREADS- NBR_RESULTS); thread++)
        results[thread] = -1;

    for (int thread = (THREADS - NBR_RESULTS); thread < THREADS; thread++) {
        int init = thread + 1;
        int result = 0;
        for (int iter = 0; iter < ITERS; iter++) {
            result += init;
            result *= init;
            result -= init;
            result /= init;
            result %= init;
        }

        results[thread] = result;
    }
    return results;
}

float* samplesFloat() {
    float *results = malloc(sizeof(float)*THREADS);

    for (int thread = 0; thread < NBR_RESULTS; thread++) {
        float init = thread + 1;
        float result = 0.0f;
        for (int iter = 0; iter < ITERS; iter++) {
            result += init;
            result *= init;
            result = sqrtf(result);
            result = logf(result);
            result -= init;
            result /= init;
            result = sinf(result);
            result = tanf(result);
            result = cosf(result);
            result = powf(result, init);
        }

        results[thread] = result;
    }

    for (int thread = NBR_RESULTS; thread < (THREADS - NBR_RESULTS); thread++)
        results[thread] = -1.0;

    for (int thread = THREADS - NBR_RESULTS; thread < THREADS; thread++) {
        float init = thread + 1;
        float result = 0.0f;
        for (int iter = 0; iter < ITERS; iter++) {
            result += init;
            result *= init;
            result = sqrtf(result);
            result = logf(result);
            result -= init;
            result /= init;
            result = sinf(result);
            result = tanf(result);
            result = cosf(result);
            result = powf(result, init);
        }

        results[thread] = result;
    }
    return results;
}

double* samplesDouble() {
    double *results = malloc(sizeof(double)*THREADS);

    for (int thread = 0; thread < NBR_RESULTS; thread++) {
        double init = thread + 1;
        double result = 0.0f;
        for (int iter = 0; iter < ITERS; iter++) {
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

        results[thread] = result;

    }

    for (int thread = NBR_RESULTS; thread < (THREADS - NBR_RESULTS); thread++)
        results[thread] = -1.0;

    for (int thread = THREADS - NBR_RESULTS; thread < THREADS; thread++) {
        double init = thread + 1;
        double result = 0.0f;
        for (int iter = 0; iter < ITERS; iter++) {
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

        results[thread] = result;

    }
    return results;
}

void verifyInt(int *is) {
    if (!VERIFY) return;
    printf("  ...verifying...");
    int *should = samplesInt();
    printf("  ...");
    for (int i = 0; i < NBR_RESULTS; i++) {
        if (should[i] != is[i]) {
            printf("ERROR: values do not match: should[%d]: %d, is[%d]: %d\n", i, should[i], i, is[i]);
            return;
        }
    }
    printf("  ...");
    for (int i = (THREADS - NBR_RESULTS); i < THREADS; i++) {
        if (should[i] != is[i]) {
            printf("ERROR: values do not match: should[%d]: %d, is[%d]: %d\n", i, should[i], i, is[i]);
            return;
        }
    }
    free(should);
    printf("  ...done!\n");
}

void verifyFloat(float *is) {
    if (!VERIFY) return;
    printf("  ...verifying...");
    float *should = samplesFloat();
    printf("  ...");
    for (int i = 0; i < NBR_RESULTS; i++) {
        if (fabs(should[i] - is[i])>EPSILON) {
            printf("ERROR: values do not match: should[%d]: %20.52f, is[%d]: %20.52f\n", i, should[i], i, is[i]);
            return;
        }
    }
    printf("  ...");
    for (int i = (THREADS - NBR_RESULTS); i < THREADS; i++) {
        if (fabs((should[i]) - is[i])>EPSILON) {
            printf("ERROR: values do not match: should[%d]: %20.52f, is[%d]: %20.52f\n", i, should[i], i, is[i]);
            return;
        }
    }
    free(should);
    printf("  ...done!\n");
}

void verifyDouble(double *is) {
    if (!VERIFY) return;
    printf("  ...verifying...");
    double *should = samplesDouble();
    printf("  ...");
    for (int i = 0; i < NBR_RESULTS; i++) {
        if (fabs(should[i] - is[i])>EPSILON) {
            printf("ERROR: values do not match: should[%d]: %20.52f, is[%d]: %20.52f\n", i, should[i], i, is[i]);
            return;
        }
    }
    printf("  ...");
    for (int i = (THREADS - NBR_RESULTS); i < THREADS; i++) {
        if (fabs(should[i] - is[i])>EPSILON) {
            printf("ERROR: values do not match: should[%d]: %20.52f, is[%d]: %20.52f\n", i, should[i], i, is[i]);
            return;
        }
    }
    free(should);
    printf("  ...done!\n");
}
