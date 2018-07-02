// Calculation of PI using Leibniz formula
// author: Schuchardt Martin, csap9442

#include "leibniz.h"

unsigned long long STEPS = 2147483648;
int REDUCTION_WORKGROUP = 256;
double PI = 3.14159265358979323846264338327950288419716939937511;
double EPSILON = 0.00001;


void verifyPI(REAL is) {
    if (isnan(is)) {
        printf("ERROR: could not calculate:\n  should be: %20.52f,\n  is:        %20.52f\n", (double)PI, (double)is);
        return;
    }

    if (fabs(PI - is)>EPSILON) {
        printf("ERROR: values do not match:\n  should be: %20.52f,\n  is:        %20.52f\n", (double)PI, (double)is);
        return;
    } else
        printf("  Accurate result: %20.52f\n", (double)is);
}
