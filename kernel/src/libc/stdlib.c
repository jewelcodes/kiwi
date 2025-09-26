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

#include <stdlib.h>

char *ulltoa(unsigned long long value, char *str, int base) {
    if(base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    char *ptr = str, *ptr1 = str, tmp_char;
    unsigned long tmp_value;
    int is_negative = 0;

    do {
        tmp_value = value % base;
        if(tmp_value < 10) {
            *ptr++ = '0' + tmp_value;
        } else {
            *ptr++ = 'a' + (tmp_value - 10);
        }

        value /= base;
    } while(value);

    if(is_negative) *ptr++ = '-';
    *ptr-- = '\0';

    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return str;
}

char *ultoa(unsigned long value, char *str, int base) {
    return ulltoa((unsigned long long)value, str, base);
}

char *ltoa(long value, char *str, int base) {
    if(value < 0) {
        *str++ = '-';
        value = -value;
    }

    return ultoa((unsigned long)value, str, base);
}

char *itoa(int value, char *str, int base) {
    return ltoa((long)value, str, base);
}

char *uitoa(unsigned int value, char *str, int base) {
    return ultoa((unsigned long)value, str, base);
}

long long atoll(const char *str) {
    long long result = 0;
    int sign = 1;

    while(*str == ' ' || *str == '\t') str++;
    if(*str == '-') {
        sign = -1;
        str++;
    } else if(*str == '+') {
        str++;
    }

    while(*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result * sign;
}

long atol(const char *str) {
    return (long)atoll(str);
}

int atoi(const char *str) {
    return (int)atol(str);
}
