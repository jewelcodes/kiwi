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

#include <kiwi/boot.h>
#include <kiwi/debug.h>
#include <stdlib.h>
#include <string.h>

#define MAX_KERNEL_ARGS             64

KiwiBootInfo kiwi_boot_info;

int parse_boot_args(char ***argv) {
    char arg[512];
    int argc = 0;
    int argi = 0;

    *argv = calloc(MAX_KERNEL_ARGS, sizeof(char *));
    if(!*argv) {
        debug_error("failed to allocate memory for kernel args");
        for(;;);
    }

    memset(arg, 0, sizeof(arg));

    int quote_mode = 0;

    for(int i = 0; i < 512; i++) {
        if(kiwi_boot_info.command_line[i] == '"') {
            quote_mode = !quote_mode;
            continue;
        }

        if(!quote_mode && (kiwi_boot_info.command_line[i] == ' ')) {
            if(argi == 0) {
                continue;
            }
        }

        if(quote_mode) {
            arg[argi++] = kiwi_boot_info.command_line[i];
            continue;
        }

        if((kiwi_boot_info.command_line[i] == ' ')
            || (kiwi_boot_info.command_line[i] == '\n')
            || (!kiwi_boot_info.command_line[i])) {
            (*argv)[argc] = strdup(arg);
            if(!(*argv)[argc]) {
                debug_error("failed to allocate memory for arg %u", argc);
                for(;;);
            }
            argc++;
            if(argc >= MAX_KERNEL_ARGS) {
                break;
            }

            argi = 0;
            memset(arg, 0, sizeof(arg));

            if(!kiwi_boot_info.command_line[i]) {
                break;
            }
        } else {
            arg[argi++] = kiwi_boot_info.command_line[i];
        }
    }

    return argc;
}
