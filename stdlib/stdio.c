#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

typedef struct ciovec_s {
    const void* buf;
    size_t buf_len;
} ciovec_t;

uint32_t fd_write(int fd, const ciovec_t* iovs, size_t iovs_len, size_t* nwritten);
uint32_t write_all(int fd, ciovec_t* iovs, size_t iovs_len);

// Buffer for formatting numbers (enough for 32-bit int + null terminator)
static char format_buffer[12];

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
static size_t int_to_str(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        return 1;
    }
    
    size_t len = 0;
    if (val < 0) {
        buf[0] = '-';
        len = 1;
        val = -val;
    }
    
    // Convert digits in reverse order
    size_t start = len;
    while (val > 0) {
        buf[len++] = '0' + (val % 10);
        val /= 10;
    }
    
    // Reverse the digits
    for (size_t i = start; i < len - 1; i++) {
        char temp = buf[i];
        buf[i] = buf[len - 1 - (i - start)];
        buf[len - 1 - (i - start)] = temp;
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
