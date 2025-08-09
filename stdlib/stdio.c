#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include <base_io.h>

// Buffer for formatting numbers (enough for 64-bit integer in decimal or hex)
static char format_buffer[32];

// Helper function to write current buffer content
static void write_buffer(char* output_buffer, size_t* output_pos) {
    if (*output_pos > 0) {
        ciovec_t iov[1];
        iov[0].buf = output_buffer;
        iov[0].buf_len = *output_pos;
        write_all(1, iov, 1);
        *output_pos = 0;
    }
}

// Helper function to convert integer to string in buffer
static size_t uint_to_str(uint64_t val, char* buf) {
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

static size_t int_to_str(int64_t val, char* buf) {
    if (val < 0) {
        buf[0] = '-';
        size_t len = uint_to_str((uint64_t)(-val), buf + 1);
        return len + 1;
    } else {
        return uint_to_str((uint64_t)val, buf);
    }
}

static size_t ptr_to_hex(uintptr_t val, char* buf) {
    // Produce minimal hex (no leading zeros) with 0x prefix later
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    const char* hex = "0123456789abcdef";
    size_t len = 0;
    while (val > 0) {
        buf[len++] = hex[val & 0xF];
        val >>= 4;
    }
    // reverse
    for (size_t i = 0; i < len / 2; i++) {
        char t = buf[i];
        buf[i] = buf[len - 1 - i];
        buf[len - 1 - i] = t;
    }
    return len;
}

int vprintf(const char* format, va_list ap) {
    // Pre-allocate a reasonable buffer size
    static char output_buffer[1024];
    size_t output_pos = 0;
    
    while (*format) {
        if (*format == '%' && *(format + 1)) {
            format++;
            int length_z = 0; // supports simple %zu
            if (*format == 'z') { // length modifier for size_t
                length_z = 1;
                format++;
            }
            switch (*format) {
                case 'd': {
                    int val = va_arg(ap, int);
                    size_t len = int_to_str(val, format_buffer);
                    
                    // Check if we have space in output buffer
                    if (output_pos + len >= sizeof(output_buffer)) {
                        // Flush current buffer to output
                        write_buffer(output_buffer, &output_pos);
                    }
                    
                    // Copy formatted number to output buffer
                    for (size_t i = 0; i < len && output_pos + i < sizeof(output_buffer); i++) {
                        output_buffer[output_pos + i] = format_buffer[i];
                    }
                    output_pos += len;
                    break;
                }
                case 'u': { // unsigned int or with prior 'z' size_t
                    uint64_t val;
                    if (length_z) {
                        val = (uint64_t)va_arg(ap, size_t);
                    } else {
                        val = (uint64_t)va_arg(ap, unsigned int);
                    }
                    size_t len = uint_to_str(val, format_buffer);
                    if (output_pos + len >= sizeof(output_buffer)) {
                        write_buffer(output_buffer, &output_pos);
                    }
                    for (size_t i = 0; i < len && output_pos + i < sizeof(output_buffer); i++) {
                        output_buffer[output_pos + i] = format_buffer[i];
                    }
                    output_pos += len;
                    break;
                }
                case 'p': { // pointer in hex
                    void* ptr = va_arg(ap, void*);
                    uintptr_t v = (uintptr_t)ptr;
                    // Write "0x"
                    if (output_pos + 2 >= sizeof(output_buffer)) {
                        write_buffer(output_buffer, &output_pos);
                    }
                    output_buffer[output_pos++] = '0';
                    output_buffer[output_pos++] = 'x';
                    size_t len = ptr_to_hex(v, format_buffer);
                    if (output_pos + len >= sizeof(output_buffer)) {
                        write_buffer(output_buffer, &output_pos);
                        // re-write 0x if flushed? Simplicity: if flushed we lose it; better flush before adding 0x above.
                        // (Simplified, assume buffer large enough for now.)
                    }
                    for (size_t i = 0; i < len && output_pos + i < sizeof(output_buffer); i++) {
                        output_buffer[output_pos + i] = format_buffer[i];
                    }
                    output_pos += len;
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(ap, int);
                    
                    if (output_pos >= sizeof(output_buffer)) {
                        // Flush current buffer to output
                        write_buffer(output_buffer, &output_pos);
                    }
                    
                    output_buffer[output_pos++] = c;
                    break;
                }
                case 's': {
                    char* str = va_arg(ap, char*);
                    if (str == NULL) {
                        str = "(null)";
                    }
                    
                    // Process string character by character
                    while (*str) {
                        if (output_pos >= sizeof(output_buffer)) {
                            // Flush current buffer to output
                            write_buffer(output_buffer, &output_pos);
                        }
                        
                        output_buffer[output_pos++] = *str;
                        str++;
                    }
                    break;
                }
                case '%': {
                    if (output_pos >= sizeof(output_buffer)) {
                        // Flush current buffer to output
                        write_buffer(output_buffer, &output_pos);
                    }
                    
                    output_buffer[output_pos++] = '%';
                    break;
                }
                default: {
                    // Unrecognized specifier: print it verbatim (best effort)
                    if (output_pos >= sizeof(output_buffer)) {
                        write_buffer(output_buffer, &output_pos);
                    }
                    output_buffer[output_pos++] = '%';
                    if (length_z) {
                        if (output_pos >= sizeof(output_buffer)) write_buffer(output_buffer, &output_pos);
                        output_buffer[output_pos++] = 'z';
                    }
                    if (output_pos >= sizeof(output_buffer)) write_buffer(output_buffer, &output_pos);
                    output_buffer[output_pos++] = *format;
                    break;
                }
            }
        } else {
            if (output_pos >= sizeof(output_buffer)) {
                // Flush current buffer to output
                write_buffer(output_buffer, &output_pos);
            }
            
            output_buffer[output_pos++] = *format;
        }
        format++;
    }
    
    // Flush any remaining data
    write_buffer(output_buffer, &output_pos);
    
    return (int)output_pos;
}

int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}
