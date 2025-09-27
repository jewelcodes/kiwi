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

#pragma once

#include <kiwi/types.h>
#include <kiwi/arch/lock.h>

#define FONT_WIDTH                  8
#define FONT_HEIGHT                 16
#define FONT_MIN_GLYPH              32
#define FONT_MAX_GLYPH              126

#define CONSOLE_WIDTH               94
#define CONSOLE_HEIGHT              34

#define BLACK                       0
#define BLUE                        1
#define GREEN                       2
#define CYAN                        3
#define RED                         4
#define MAGENTA                     5
#define BROWN                       6
#define LIGHT_GRAY                  7
#define DARK_GRAY                   8
#define LIGHT_BLUE                  9
#define LIGHT_GREEN                 10
#define LIGHT_CYAN                  11
#define LIGHT_RED                   12
#define LIGHT_MAGENTA               13
#define YELLOW                      14
#define WHITE                       15

typedef struct KernelTerminal {
    lock_t lock;
    u32 width, height, pitch;
    u8 bpp;
    u32 bg, fg;
    u16 x, y;
    u32 *front_buffer;
    u32 *back_buffer;
} KernelTerminal;

extern KernelTerminal kernel_terminal;
extern const u8 font[];
extern const u32 palette[];

void tty_clear(void);
void tty_putchar(char c);
void tty_puts(const char *str);
