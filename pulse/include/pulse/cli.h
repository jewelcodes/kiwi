/*
 * pulse - a highly scalable SSD-first file system with predictable logarithmic
 * bounds across all operations
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

#include <stdio.h>

#define ESC_RESET               "\e[0m"
#define ESC_BOLD                "\e[1m"
#define ESC_RED                 "\e[31m"
#define ESC_GREEN               "\e[32m"
#define ESC_YELLOW              "\e[33m"
#define ESC_BLUE                "\e[34m"
#define ESC_MAGENTA             "\e[35m"
#define ESC_CYAN                "\e[36m"
#define ESC_WHITE               "\e[37m"
#define ESC_GRAY                "\e[90m"
#define ESC_BOLD_RED            "\e[1;31m"
#define ESC_BOLD_GREEN          "\e[1;32m"
#define ESC_BOLD_YELLOW         "\e[1;33m"
#define ESC_BOLD_BLUE           "\e[1;34m"
#define ESC_BOLD_MAGENTA        "\e[1;35m"
#define ESC_BOLD_CYAN           "\e[1;36m"
#define ESC_BOLD_WHITE          "\e[1;37m"
#define ESC_BOLD_GRAY           "\e[1;90m"
#define ESC_BG_RED              "\e[41m"
#define ESC_BG_GREEN            "\e[42m"
#define ESC_BG_YELLOW           "\e[43m"
#define ESC_BG_BLUE             "\e[44m"
#define ESC_BG_MAGENTA          "\e[45m"
#define ESC_BG_CYAN             "\e[46m"
#define ESC_BG_WHITE            "\e[47m"
#define ESC_BG_GRAY             "\e[49m"
#define ESC_BOLD_BG_RED         "\e[1;41m"
#define ESC_BOLD_BG_GREEN       "\e[1;42m"
#define ESC_BOLD_BG_YELLOW      "\e[1;43m"
#define ESC_BOLD_BG_BLUE        "\e[1;44m"
#define ESC_BOLD_BG_MAGENTA     "\e[1;45m"
#define ESC_BOLD_BG_CYAN        "\e[1;46m"
#define ESC_BOLD_BG_WHITE       "\e[1;47m"
#define ESC_BOLD_BG_GRAY        "\e[1;49m"

struct Command {
    const char *name;
    const char *description;
    int (*function)(int argc, char **argv);
};

extern FILE *__disk;
extern u16 __block_size;
extern u8 __fanout;
extern u64 __block_count;
extern struct Command commands[];
extern char *__image_name;

int command_line(const char *name);
int script(int argc, char **argv);

int mount_command(int argc, char **argv);
int create_command(int argc, char **argv);
int test_command(int argc, char **argv);
