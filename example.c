#include <math.h>
#include <stdint.h>
#include <string.h>
#include <wasi.h>
#include <example.h>


void print_string(const char* str, uint32_t len) {
    ciovec_t iov = { (void *)str, len };
    size_t nwritten;
    fd_write(1, &iov, 1, &nwritten);
}

void log_message(const char *text) {
    size_t len = strlen(text);
    char newline = '\n';
    print_string(text, len);
    print_string(&newline, 1);
}

// Export the add function for JavaScript and Wasmtime
int add(int a, int b) {
    log_message("Adding two numbers");
    return a + b;
}

double mysin(double a) {
    log_message("Calculating sine");
    return sin(a);
}
