#include <math.h>

// Export the add function for JavaScript and Wasmtime
int add(int a, int b) {
    return a + b;
}

double mysin(double a) {
    return sin(a);
}
