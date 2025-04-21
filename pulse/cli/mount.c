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
#include <string.h>
#include <stdlib.h>

Mountpoint *mountpoint = NULL;

int mount_command(int argc, char **argv) {
    if(argc != 2) {
        printf(ESC_BOLD_CYAN "usage:" ESC_RESET " mount <image>\n");
        printf(ESC_BOLD_CYAN "example:" ESC_RESET " mount /path/to/image.hdd\n");
        return 1;
    }

    if(mountpoint && mountpoint->name) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " unmount %s first\n", mountpoint->name);
        return 1;
    }

    if(mountpoint) {
        free(mountpoint);
        mountpoint = NULL;
    }

    mountpoint = malloc(sizeof(Mountpoint));
    if(!mountpoint) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for mountpoint\n");
        return 1;
    }

    char *duplicate = strdup(argv[1]);
    if(!duplicate) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for image name\n");
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    char *token = strtok(duplicate, "/");
    while(token) {
        char *next = strtok(NULL, "/");
        if(!next) break;
        token = next;
    }

    if(!token)
        token = argv[1];

    mountpoint->name = token;
    printf(ESC_BOLD_GREEN "mount:" ESC_RESET " ✅ mounted disk image %s\n", argv[1]);
    return 0;
}
