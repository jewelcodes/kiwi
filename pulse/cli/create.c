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

#include <pulse/pulse.h>
#include <pulse/cli.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int create_command(int argc, char **argv) {
    if(argc < 2 || argc > 6) {
        printf(ESC_BOLD_CYAN "usage:" ESC_RESET " create <flags|null> <image> <size|10m> <blocksize|4096> <fanout|16>\n");
        printf(ESC_BOLD_CYAN "flags:" ESC_RESET " -m, --mount  mount after creation\n");
        printf(ESC_BOLD_CYAN "example:" ESC_RESET " create -m /path/to/image.hdd 50G\n");
        return 1;
    }

    int mount = !strcmp(argv[1], "-m") || !strcmp(argv[1], "--mount");

    if(mount && mountpoint && mountpoint->name) {
        printf(ESC_BOLD_RED "create:" ESC_RESET " unmount %s first\n", mountpoint->name);
        return 1;
    }

    int img_index = mount ? 2 : 1;

    usize size = img_index+1 < argc ? atoi(argv[img_index+1]) : 1024*1024*10;
    usize block_size = img_index+2 < argc ? atoi(argv[img_index+2]) : DEFAULT_BLOCK_SIZE;
    usize fanout = img_index+3 < argc ? atoi(argv[img_index+3]) : DEFAULT_FANOUT_FACTOR;

    if(img_index+1 < argc) {
        char unit = argv[img_index+1][strlen(argv[img_index+1]) - 1];
        switch(unit) {
        case 'b':
        case 'B':
            break;
        case 'k':
        case 'K':
            size *= 1024;
            break;
        case 'g':
        case 'G':
            size *= 1024*1024*1024;
            break;
        case 'm':
        case 'M':
        default:
            size *= 1024*1024;
            break;
        }
    }

    if(!size) {
        printf(ESC_BOLD_RED "create:" ESC_RESET " invalid size %zu\n", size);
        return 1;
    }

    if(block_size < 4096 || block_size > 512*1024 || (block_size & (block_size - 1))) {
        printf(ESC_BOLD_RED "create:" ESC_RESET " invalid block size %zu\n", block_size);
        return 1;
    }

    if(fanout < 8 || fanout > 64 || (fanout & (fanout - 1))) {
        printf(ESC_BOLD_RED "create:" ESC_RESET " invalid fanout %zu\n", fanout);
        return 1;
    }

    printf(ESC_BOLD_CYAN "create:" ESC_RESET " creating disk image %s with size %zu %s\n",
        argv[img_index],
        size >= 1024*1024*1024 ? size/(1024*1024*1024) : size >= 1024*1024 ? size/(1024*1024) : size >= 1024 ? size/1024 : size,
        size >= 1024*1024*1024 ? "GB" : size >= 1024*1024 ? "MB" : size >= 1024 ? "KB" : "B");
    
    int status = format(argv[img_index], size, block_size, fanout);
    if(status) {
        printf(ESC_BOLD_RED "create:" ESC_RESET " failed to create disk image %s\n", argv[img_index]);
        return status;
    }

    char *path = strdup(argv[img_index]);
    if(!path) {
        printf(ESC_BOLD_RED "create:" ESC_RESET " failed to allocate memory for image name\n");
        return 1;
    }

    if(mount) {
        char *args[] = { "mount", path };
        int status = mount_command(2, args);
        free(path);
        if(status) {
            printf(ESC_BOLD_RED "create:" ESC_RESET " failed to mount disk image %s\n", path);
            return status;
        }
    }

    printf(ESC_BOLD_GREEN "create:" ESC_RESET " ✅ created disk image %s\n", argv[img_index]);
    return 0;
}