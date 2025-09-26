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

#include <kiwi/tty.h>
#include <kiwi/arch/atomic.h>
#include <stddef.h>

KernelTerminal kernel_terminal = {
    .lock = LOCK_INITIAL,
    .width = 0,
    .height = 0,
    .pitch = 0,
    .bpp = 0,
    .bg = 0,
    .fg = 0,
    .x = 0,
    .y = 0,
    .front_buffer = NULL,
    .back_buffer = NULL
};

const u32 palette[] = {
    0x101010,       // black
    0x3B5BA7,       // blue
    0x6CA45A,       // green
    0x4AAE9E,       // cyan
    0xC74B4B,       // red
    0xB65CA8,       // magenta
    0x8F673D,       // brown
    0xCFCFCF,       // light gray
    0x5C5C5C,       // dark gray
    0x547FD4,       // light blue
    0x9BD97C,       // light green
    0x6FD5C4,       // light cyan
    0xE36E6E,       // light red
    0xD47CC9,       // light magenta
    0xE9E46C,       // yellow
    0xF5F5F5        // white
};

void tty_clear(void) {
    if(!kernel_terminal.front_buffer)
        return;

    arch_spinlock_acquire(&kernel_terminal.lock);

    u32 pitch = kernel_terminal.pitch;
    
    for(int i = 0; i < kernel_terminal.height; i++) {
        u32 *row = (u32 *)((uptr) kernel_terminal.front_buffer + i * pitch);
        for(int j = 0; j < kernel_terminal.width; j++) {
            row[j] = kernel_terminal.bg;
        }
    }

    kernel_terminal.x = 0;
    kernel_terminal.y = 0;
    arch_spinlock_release(&kernel_terminal.lock);
}

void tty_putchar(char c) {
    if(!kernel_terminal.front_buffer)
        return;

    arch_spinlock_acquire(&kernel_terminal.lock);

    if(c == '\r') {
        kernel_terminal.x = 0;
        arch_spinlock_release(&kernel_terminal.lock);
        return;
    } else if(c == '\n') {
        kernel_terminal.x = 0;
        kernel_terminal.y++;
        goto check_boundaries;
    } else if(c == '\t') {
        kernel_terminal.x += 4 - (kernel_terminal.x % 4);
        goto check_boundaries;
    }

    if(c < FONT_MIN_GLYPH || c > FONT_MAX_GLYPH) c = ' ';
    const u8 *font_data = &font[(c - FONT_MIN_GLYPH) * FONT_HEIGHT];

    u32 y = (kernel_terminal.y * FONT_HEIGHT)
        + ((kernel_terminal.height/2
        - (CONSOLE_HEIGHT*FONT_HEIGHT)/2));
    u32 x = (kernel_terminal.x * FONT_WIDTH)
        + ((kernel_terminal.width/2
        - (CONSOLE_WIDTH*FONT_WIDTH)/2));
    u32 pitch = kernel_terminal.pitch;

    for(int j = 0; j < FONT_HEIGHT; j++) {
        u8 data = font_data[j];
        u32 *row = (u32 *)((uptr) kernel_terminal.front_buffer + (y + j) * pitch + x * 4);
        for(int i = 0; i < FONT_WIDTH; i++) {
            u32 color = (data & 0x80) ? kernel_terminal.fg : kernel_terminal.bg;
            row[i] = color;
            data <<= 1;
        }
    }

    kernel_terminal.x++;

check_boundaries:
    if(kernel_terminal.x >= CONSOLE_WIDTH) {
        kernel_terminal.x = 0;
        kernel_terminal.y++;
    }

    if(kernel_terminal.y >= CONSOLE_HEIGHT) {
        // TODO: scroll up instead
        kernel_terminal.x = 0;
        kernel_terminal.y = 0;
    }

    arch_spinlock_release(&kernel_terminal.lock);
}

void tty_puts(const char *str) {
    while(*str) {
        tty_putchar(*str++);
    }
}
