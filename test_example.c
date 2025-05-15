#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <example.h>

int main() {
    int r = add(2, 3);
    printf("add(2,3) = %d\n", r);
    if (r != 5) abort();

    double s = mysin(1.5);
    printf("mysin(1.5) = %f\n", s);
    if (fabs(s-0.9974949866040547) > 1e-12) abort();

    return 0;
}
