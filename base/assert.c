#include <base/assert.h>
#include <base/wasi.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/numconv.h>

// Simple string output helper
static void write_str(const char *str) {
    size_t len = strlen(str);
    ciovec_t iov = {.buf = str, .buf_len = len};
    write_all(1, &iov, 1);
}

// Simple number output helper
static void write_uint(unsigned int num) {
    char buf[32];
    size_t len = uint64_to_str(num, buf);
    ciovec_t iov = {.buf = buf, .buf_len = len};
    write_all(1, &iov, 1);
}

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    write_str("Assertion failed: (");
    write_str(assertion);
    write_str(") at '");
    write_str(file);
    write_str(":");
    write_uint(line);
    write_str("' in function '");
    write_str(function);
    write_str("'\n");
    wasi_proc_exit(1);
}
