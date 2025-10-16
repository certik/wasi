#include <base/numconv.h>

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
