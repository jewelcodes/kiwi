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

int mount_command(int argc, char **argv) {
    if(argc != 2) {
        printf(ESC_BOLD_CYAN "usage:" ESC_RESET " mount <image>\n");
        printf(ESC_BOLD_CYAN "example:" ESC_RESET " mount /path/to/image.hdd\n");
        return 1;
    }

    if(__image_name) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " unmount %s first\n", __image_name);
        return 1;
    }

    char *token = strtok(argv[1], "/");
    while(token) {
        char *next = strtok(NULL, "/");
        if(!next) break;
        token = next;
    }

    if(!token)
        token = argv[1];
    
    char *duplicate = strdup(token);
    if(!duplicate) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for image name\n");
        return 1;
    }

    __image_name = duplicate;
    printf(ESC_BOLD_GREEN "mount:" ESC_RESET " ✅ mounted disk image %s\n", __image_name);
    return 0;
}
