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

#include <boot/output.h>
#include <boot/bios.h>

Display display = {
    .vbe_enabled = 0,
    .current_mode = NULL
};

static Registers output_regs;

/*
 * print:
 * @brief: prints a string to the screen
 * @param: str: null-terminated string to print
 */

void print(const char *str) {
    if(display.vbe_enabled && display.current_mode) return; // todo
    else bios_print(str);
}

/*
 * bios_print:
 * @brief: prints a string using BIOS interrupts
 * @param: str: null-terminated string to print
 */

void bios_print(const char *str) {
    while(*str) {
        if(*str == '\n') {
            output_regs.eax = 0x0E00 | '\r'; // carriage return
            bios_int(0x10, &output_regs);
        }
        output_regs.eax = 0x0E00 | *str++;
        bios_int(0x10, &output_regs);
    }
}