#include <stdint.h>
#include <stddef.h>

#include <base/mem.h>
#include <base/exit.h>
#include <wasi.h>
#include <buddy.h>
#include <stdlib.h>

void* malloc(size_t size) {
    return buddy_alloc(size);
}

void free(void* ptr) {
    buddy_free(ptr);
}

// exit() and abort() now reuse implementations from base/exit.c

// Linear Congruential Generator (LCG)
// Parameters: a = 1103515245, c = 12345, m = 2^31
static uint32_t rand_state = 1;

void srand(int seed) {
    rand_state = (uint32_t)seed;
}

int rand() {
    rand_state = (rand_state * 1103515245u + 12345u) & 0x7FFFFFFF;
    return (int)rand_state;
}

// Helper function to convert integer to string in buffer
static size_t uint_to_str_snprintf(uint64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }

    size_t len = 0;
    // Convert digits in reverse order
    while (val > 0) {
        buf[len++] = (char)('0' + (val % 10));
        val /= 10;
    }
    // Reverse
    for (size_t i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    return len;
}

static size_t int_to_str_snprintf(int64_t val, char* buf) {
    if (val < 0) {
        buf[0] = '-';
        size_t len = uint_to_str_snprintf((uint64_t)(-val), buf + 1);
        return len + 1;
    } else {
        return uint_to_str_snprintf((uint64_t)val, buf);
    }
}

static size_t double_to_str_snprintf(double val, char* buf, int precision) {
    // Simple implementation: handle sign, integer part, decimal point, fractional part
    size_t pos = 0;

    if (val < 0) {
        buf[pos++] = '-';
        val = -val;
    }

    // Integer part
    int64_t int_part = (int64_t)val;
    pos += int_to_str_snprintf(int_part, buf + pos);

    if (precision > 0) {
        buf[pos++] = '.';

        // Fractional part
        double frac_part = val - (double)int_part;
        for (int i = 0; i < precision; i++) {
            frac_part *= 10;
            int digit = (int)frac_part;
            buf[pos++] = '0' + digit;
            frac_part -= digit;
        }
    }

    return pos;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    if (size == 0) return 0;

    va_list args;
    va_start(args, format);

    size_t pos = 0;
    const char* p = format;
    char temp_buf[32];

    while (*p && pos < size - 1) {
        if (*p == '%' && *(p + 1)) {
            p++;
            int precision = -1;

            // Check for precision specifier (. followed by * or digits)
            if (*p == '.') {
                p++;
                if (*p == '*') {
                    // Dynamic precision from va_args
                    precision = va_arg(args, int);
                    p++;
                } else {
                    precision = 0;
                    while (*p >= '0' && *p <= '9') {
                        precision = precision * 10 + (*p - '0');
                        p++;
                    }
                }
            }

            switch (*p) {
                case 'd': {
                    int val = va_arg(args, int);
                    size_t len = int_to_str_snprintf(val, temp_buf);
                    size_t copy_len = (pos + len < size - 1) ? len : (size - 1 - pos);
                    memcpy(str + pos, temp_buf, copy_len);
                    pos += copy_len;
                    break;
                }
                case 'f': {
                    double val = va_arg(args, double);
                    if (precision < 0) precision = 6;
                    size_t len = double_to_str_snprintf(val, temp_buf, precision);
                    size_t copy_len = (pos + len < size - 1) ? len : (size - 1 - pos);
                    memcpy(str + pos, temp_buf, copy_len);
                    pos += copy_len;
                    break;
                }
                case 's': {
                    char* s = va_arg(args, char*);
                    if (s == NULL) s = "(null)";
                    while (*s && pos < size - 1) {
                        str[pos++] = *s++;
                    }
                    break;
                }
                case '%': {
                    str[pos++] = '%';
                    break;
                }
                default: {
                    if (pos < size - 1) str[pos++] = '%';
                    if (pos < size - 1) str[pos++] = *p;
                    break;
                }
            }
        } else {
            str[pos++] = *p;
        }
        p++;
    }

    str[pos] = '\0';
    va_end(args);
    return (int)pos;
}
