/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2025 Omar Elghoul
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Partial implementation of printf */

#include <kiwi/types.h>

/*
 * format = %[flags][width][.precision][length]specifier
 * flags: -, +, space, #, 0
 *   - = left-justify
 *   + = force showing sign
 *   space = prefix a space for positive numbers
 *   # = alternate form (0x for hex, 0 for octal)
 *   0 = zero-padding
 * width: (number) minimum field width
 * precision: .(number) for floats, max string length for strings
 * length: h, l, hh, ll, j, z, t
 *   h = short
 *   l = long
 *   hh = char
 *   ll = long long
 *   j = intmax_t
 *   z = size_t
 *   t = ptrdiff_t
 * specifier: c, s, p, d, i, u, o, x, X
 */

#define STATE_NORMAL                0
#define STATE_FLAGS                 1
#define STATE_WIDTH                 2
#define STATE_PRECISION             3
#define STATE_LENGTH                4
#define STATE_SPECIFIER             5

#define LENGTH_DEFAULT              0
#define LENGTH_SHORT                1
#define LENGTH_LONG                 2
#define LENGTH_CHAR                 3
#define LENGTH_LONG_LONG            4
#define LENGTH_INTMAX_T             5
#define LENGTH_SIZE_T               6
#define LENGTH_PTRDIFF_T            7

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <boot/output.h>

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    int state = STATE_NORMAL;
    size--;
    int len = 0;
    usize output_index = 0;
    usize format_index = 0;
    int length_modifier;

    for(; format[format_index]; format_index++) {
        switch(state) {
            case STATE_NORMAL: {
                if(format[format_index] == '%') {
                    state = STATE_FLAGS;
                    length_modifier = LENGTH_DEFAULT;
                } else {
                    if(output_index < size) {
                        str[output_index] = format[format_index];
                        len++;
                    }
                    output_index++;
                }
                break;
            }
            
            case STATE_FLAGS: {
                switch(format[format_index]) {
                    /* todo */
                    default:
                        state = STATE_WIDTH;
                        format_index--;
                        break;
                }
                break;
            }

            case STATE_WIDTH: {
                if(format[format_index] >= '0' && format[format_index] <= '9') {
                    /* todo */
                } else {
                    state = STATE_PRECISION;
                    format_index--;
                }
                break;
            }

            case STATE_PRECISION: {
                if(format[format_index] == '.') {
                    /* todo */
                } else {
                    state = STATE_LENGTH;
                    format_index--;
                }
                break;
            }

            case STATE_LENGTH: {
                switch(format[format_index]) {
                    case 'h':
                        if(length_modifier == LENGTH_SHORT) {
                            length_modifier = LENGTH_CHAR;
                        } else {
                            length_modifier = LENGTH_SHORT;
                        }
                        break;
                    case 'l':
                        if(length_modifier == LENGTH_LONG) {
                            length_modifier = LENGTH_LONG_LONG;
                        } else {
                            length_modifier = LENGTH_LONG;
                        }
                        break;
                    case 'j':
                        length_modifier = LENGTH_INTMAX_T;
                        break;
                    case 'z':
                        length_modifier = LENGTH_SIZE_T;
                        break;
                    case 't':
                        length_modifier = LENGTH_PTRDIFF_T;
                        break;
                    default:
                        state = STATE_SPECIFIER;
                        format_index--;
                        break;
                }
                break;
            }

            case STATE_SPECIFIER: {
                switch(format[format_index]) {
                    case 'c': {
                        char c = (char) va_arg(ap, int);
                        if(output_index < size) {
                            str[output_index] = c;
                            len++;
                        }
                        output_index++;
                        break;
                    }

                    case 'd': {
                        long long int value;
                        char buffer[32];
                        if(length_modifier == LENGTH_CHAR) {
                            value = (char) va_arg(ap, int);
                        } else if(length_modifier == LENGTH_SHORT) {
                            value = (short) va_arg(ap, int);
                        } else if(length_modifier == LENGTH_LONG) {
                            value = va_arg(ap, long);
                        } else if(length_modifier == LENGTH_LONG_LONG) {
                            value = va_arg(ap, long long);
                        } else if(length_modifier == LENGTH_INTMAX_T) {
                            value = va_arg(ap, intmax_t);
                        } else if(length_modifier == LENGTH_SIZE_T) {
                            value = (long long) va_arg(ap, size_t);
                        } else if(length_modifier == LENGTH_PTRDIFF_T) {
                            value = (long long) va_arg(ap, ptrdiff_t);
                        } else {
                            value = va_arg(ap, int);
                        }
                        ltoa(value, buffer, 10);
                        for(usize i = 0; buffer[i]; i++) {
                            if(output_index < size) {
                                str[output_index] = buffer[i];
                                len++;
                            }
                            output_index++;
                        }
                        break;
                    }
                }
            }
        }
    }

    str[len] = 0;
    return len;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, (size_t) -1, format, ap);
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsprintf(str, format, ap);
    va_end(ap);
    return ret;
}

int vprintf(const char *format, va_list ap) {
    char buffer[1024];
    int len = vsnprintf(buffer, sizeof(buffer), format, ap);
    print(buffer);
    return len;
}

int printf(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}
