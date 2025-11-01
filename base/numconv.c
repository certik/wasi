#include <base/numconv.h>
#include <base/stdarg.h>

size_t uint64_to_str(uint64_t val, char* buf) {
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

size_t int64_to_str(int64_t val, char* buf) {
    if (val < 0) {
        buf[0] = '-';
        size_t len = uint64_to_str((uint64_t)(-val), buf + 1);
        return len + 1;
    } else {
        return uint64_to_str((uint64_t)val, buf);
    }
}

size_t int_to_str(int val, char* buf) {
    return int64_to_str((int64_t)val, buf);
}

size_t double_to_str(double val, char* buf, int precision) {
    // Simple implementation: handle sign, integer part, decimal point, fractional part
    size_t pos = 0;

    if (val < 0) {
        buf[pos++] = '-';
        val = -val;
    }

    // Integer part
    int64_t int_part = (int64_t)val;
    pos += int64_to_str(int_part, buf + pos);

    // Default precision is 6 if not specified
    if (precision < 0) precision = 6;

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

// vsnprintf implementation (called by snprintf)
int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    if (size == 0) return 0;

    size_t pos = 0;
    const char* p = format;
    char temp_buf[32];

    while (*p && pos < size - 1) {
        if (*p == '%' && *(p + 1)) {
            p++;

            // Check for precision specifier (e.g., %.2f)
            int precision = -1;
            if (*p == '.') {
                p++;
                precision = 0;
                while (*p >= '0' && *p <= '9') {
                    precision = precision * 10 + (*p - '0');
                    p++;
                }
            }

            switch (*p) {
                case 'd':
                case 'u': {
                    int val = va_arg(args, int);
                    size_t len = int_to_str(val, temp_buf);
                    size_t copy_len = (pos + len < size - 1) ? len : (size - 1 - pos);
                    for (size_t i = 0; i < copy_len; i++) {
                        str[pos++] = temp_buf[i];
                    }
                    break;
                }
                case 'f': {
                    double val = va_arg(args, double);
                    if (precision < 0) precision = 6;
                    size_t len = double_to_str(val, temp_buf, precision);
                    size_t copy_len = (pos + len < size - 1) ? len : (size - 1 - pos);
                    for (size_t i = 0; i < copy_len; i++) {
                        str[pos++] = temp_buf[i];
                    }
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
                default:
                    // Unknown format specifier, just skip it
                    break;
            }
            p++;
        } else {
            str[pos++] = *p++;
        }
    }

    str[pos] = '\0';
    return (int)pos;
}

// Simple snprintf implementation for base/
// Supports: %d, %u, %f, %.Nf, %s
int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}
