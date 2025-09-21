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

static Registers output_regs;
static void bios_print(const char *str);
static void fb_putc(char c);
static void fb_print(const char *str);

/*
 * print:
 * @brief: prints a string to the screen
 * @param: str: null-terminated string to print
 */

void print(const char *str) {
    if(display.vbe_enabled && display.current_mode) fb_print(str);
    else bios_print(str);
}

static void bios_print(const char *str) {
    while(*str) {
        if(*str == '\n') {
            output_regs.eax = 0x0E00 | '\r'; // carriage return
            bios_int(0x10, &output_regs);
        }
        output_regs.eax = 0x0E00 | *str++;
        bios_int(0x10, &output_regs);
    }
}

static void fb_putc(char c) {
    if(c == '\n') {
        display.x = 0;
        display.y++;
        if(display.y >= CONSOLE_HEIGHT) {
            display.y = 0;
        }
        return;
    }

    if(display.x >= CONSOLE_WIDTH) {
        display.x = 0;
        display.y++;
        if(display.y >= CONSOLE_HEIGHT) {
            display.y = 0;
        }
    }

    if(c < FONT_MIN_GLYPH || c > FONT_MAX_GLYPH) c = ' ';
    const u8 *font_data = &font[(c - FONT_MIN_GLYPH) * FONT_HEIGHT];

    u32 y = (display.y * FONT_HEIGHT)
        + ((display.current_mode->height/2
        - (CONSOLE_HEIGHT*FONT_HEIGHT)/2));
    u32 x = (display.x * FONT_WIDTH)
        + ((display.current_mode->width/2
        - (CONSOLE_WIDTH*FONT_WIDTH)/2));
    u32 pitch = display.current_mode->pitch;

    for(int row = 0; row < FONT_HEIGHT; row++) {
        for(int col = 0; col < FONT_WIDTH; col++) {
            u32 color = (font_data[row] & (0x80 >> col)) ? display.fg : display.bg;
            u32 *pixel = (u32 *) (display.current_mode->framebuffer
                + (y + row) * pitch + (x + col) * 4);
            *pixel = color;
        }
    }

    display.x++;
}

static void fb_print(const char *str) {
    while(*str) {
        fb_putc(*str++);
    }
}

void clear_screen(void) {
    if(!display.vbe_enabled || !display.current_mode) return;

    u32 *fb = (u32 *) display.current_mode->framebuffer;

    for(int y = 0; y < display.current_mode->height; y++) {
        for(int x = 0; x < display.current_mode->width; x++) {
            fb[x] = display.bg;
        }

        fb += display.current_mode->pitch / 4;
    }
}

void dim_screen(void) {
    if(!display.vbe_enabled || !display.current_mode) return;

    u32 *fb = (u32 *) display.current_mode->framebuffer;

    for(int y = 0; y < display.current_mode->height; y++) {
        for(int x = 0; x < display.current_mode->width; x++) {
            fb[x] = (fb[x] >> 1) & 0x7F7F7F;
        }

        fb += display.current_mode->pitch / 4;
    }
}

void fill_rect(u32 x, u32 y, u32 width, u32 height, u32 color) {
    if(!display.vbe_enabled || !display.current_mode) return;

    if(x + width > display.current_mode->width)
        width = display.current_mode->width - x;
    if(y + height > display.current_mode->height)
        height = display.current_mode->height - y;

    u32 *fb = (u32 *) (display.current_mode->framebuffer
        + y * display.current_mode->pitch + x * 4);

    for(u32 row = 0; row < height; row++) {
        for(u32 col = 0; col < width; col++) {
            fb[col] = color;
        }

        fb += display.current_mode->pitch / 4;
    }
}
