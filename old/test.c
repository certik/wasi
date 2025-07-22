// clang -nostdinc -nostdlib -e _start -arch arm64 -lSystem -o test test.c

// Custom strlen to avoid stdlib
unsigned long my_strlen(const char *str) {
    unsigned long len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

long write(int fd, const void *buf, unsigned long count);
void exit(int status);

// Write to stdout using syscall
int write_to_stdout(const char *msg, unsigned long len) {
    return write(1, msg, len);
}

// Entry point
__attribute__((visibility("default")))
void start() {
    const char *message = "Hello, world!\n";
    unsigned long len = my_strlen(message);
    
    // Write message
    if (write_to_stdout(message, len) != 0) {
        exit(1);
    }

    exit(0);
}
