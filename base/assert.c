#include <base/assert.h>
#include <base/base_io.h>
#include <base/mem.h>
#include <base/wasi.h>
#include <base/numconv.h>

/* After proper strings and formatting, this should just be:

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    printf("Assertion failed: (%s) at '%s:%u' in function '%s'\n", assertion, file, line, function);
    exit(1);
}

However, we do not want to depend on arenas working, we only want to use the
lower-level API, no other base dependencies, so that asser() can be used
anywhere in base.
*/

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    // Simple assertion failure handler using only base/ dependencies
    const char *msg1 = "Assertion failed: (";
    const char *msg2 = ") at '";
    const char *msg3 = ":";
    char p[32]; size_t p_len = int_to_str(line, p); p[p_len] = '\0';
    const char *msg4 = "' in function '";
    const char *msg5 = "'\n";

    ciovec_t iovs[9];
    iovs[0].buf = msg1;
    iovs[0].buf_len = strlen(msg1);
    iovs[1].buf = assertion;
    iovs[1].buf_len = strlen(assertion);
    iovs[2].buf = msg2;
    iovs[2].buf_len = strlen(msg2);
    iovs[3].buf = file;
    iovs[3].buf_len = strlen(file);
    iovs[4].buf = msg3;
    iovs[4].buf_len = strlen(msg3);
    iovs[5].buf = p;
    iovs[5].buf_len = strlen(p);
    iovs[6].buf = msg4;
    iovs[6].buf_len = strlen(msg4);
    iovs[7].buf = function;
    iovs[7].buf_len = strlen(function);
    iovs[8].buf = msg5;
    iovs[8].buf_len = strlen(msg5);

    write_all(WASI_STDERR_FD, iovs, 9);
    wasi_proc_exit(1);
}
