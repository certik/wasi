#include <stdio.h>

// Export the add function for JavaScript and Wasmtime
int add(int a, int b) {
    return a + b;
}

// Main function for Wasmtime testing
int main() {
    printf("Testing add(3, 4) = %d\n", add(3, 4));
    return 0;
}
