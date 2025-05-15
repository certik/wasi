typedef unsigned long long uint64_t;

static inline double copysign(double x, double y) {
    uint64_t xi = *(uint64_t*)&x;
    uint64_t yi = *(uint64_t*)&y;
    xi &= ~(1ULL << 63);  // Clear sign bit
    xi |= yi & (1ULL << 63);  // Set sign bit to y's sign
    return *(double*)&xi;
}

static inline double round(double x) {
    return (int)(x + 0.5 * copysign(1.0, x));
}

// Accurate on [-pi/2,pi/2] to about 1e-15 in relative accuracy
static inline double kernel_sin(double x) {
    const double S1 = 1.0;
    const double S2 = -0.16666666666665748417;
    const double S3 = 8.333333333260810195e-3;
    const double S4 = -1.9841269819408224684e-4;
    const double S5 = 2.7557315969010714494e-6;
    const double S6 = -2.5051843446312301534e-8;
    const double S7 = 1.6047020166520616231e-10;
    const double S8 = -7.360938387054769116e-13;
    double z = x * x;
    return x * (S1 + z * (S2 + z * (S3 + z * (S4 + z * (S5 + z * (S6 + z * (S7 + z * S8)))))));
}

static inline double fast_sin(double x) {
    const double pi = 3.1415926535897932384626433832795;
    int N = (int) round(x / pi);
    double y = x - N * pi;
    if (N % 2 == 1) y = -y;
    return kernel_sin(y);
}

// Export the add function for JavaScript and Wasmtime
int add(int a, int b) {
    return a + b;
}

double mysin(double a) {
    return fast_sin(a);
}
