#include <base/base_io.h>
#include <base/base_types.h>
#include <base/mem.h>
#include <base/wasi.h>
#include <base/numconv.h>

uint32_t write_all(int fd, ciovec_t* iovs, size_t iovs_len) {
    size_t i;
    size_t nwritten;
    uint32_t ret;

    for (i = 0; i < iovs_len; ) {
        ret = wasi_fd_write(fd, &iovs[i], iovs_len - i, &nwritten);
        if (ret != 0) {
            return ret; // Return error code
        }

        // Advance through the iovecs based on how much was written
        while (nwritten > 0 && i < iovs_len) {
            if (nwritten >= iovs[i].buf_len) {
                nwritten -= iovs[i].buf_len;
                i++;
            } else {
                iovs[i].buf = (const uint8_t*)iovs[i].buf + nwritten;
                iovs[i].buf_len -= nwritten;
                nwritten = 0;
            }
        }
    }
    return 0; // Success
}

void writeln(int fd, char* text) {
    const char *msg1 = text;
    const char *msg2 = "\n";

    ciovec_t iovs[2];
    iovs[0].buf = msg1;
    iovs[0].buf_len = strlen(msg1);
    iovs[1].buf = msg2;
    iovs[1].buf_len = strlen(msg2);

    write_all(fd, iovs, 2);
}

void writeln_int(int fd, char* text, int n) {
    const char *msg1 = text;
    const char *msg2 = " ";
    char p[32]; size_t p_len = int_to_str(n, p); p[p_len] = '\0';
    const char *msg3 = "\n";

    ciovec_t iovs[4];
    iovs[0].buf = msg1;
    iovs[0].buf_len = strlen(msg1);
    iovs[1].buf = msg2;
    iovs[1].buf_len = strlen(msg2);
    iovs[2].buf = p;
    iovs[2].buf_len = strlen(p);
    iovs[3].buf = msg3;
    iovs[3].buf_len = strlen(msg3);

    write_all(WASI_STDERR_FD, iovs, 4);
}
