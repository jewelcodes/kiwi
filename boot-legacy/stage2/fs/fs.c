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

#include <boot/fs.h>
#include <boot/disk.h>
#include <fs/ext2.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

static int device_from_path(const char *path, Drive **drive, int *partition) {
    if(memcmp(path, "boot:", 5) == 0) {
        *drive = &boot_drive;
        *partition = 0;
        return 6;
    }

    if(path[0] == 's' && path[1] == 'd' && path[3] == 'p') {
        if(!isdigit(path[2]) || !isdigit(path[4])) {
            return -1;
        }
        
        int drive_num = path[2] - '0';
        int part_num = path[4] - '0';
        if(drive_num < 0 || drive_num >= drive_count || part_num < 0 || part_num >= 4) {
            return -1;
        }

        *drive = &drives[drive_num];
        *partition = part_num;
        return 6;
    }

    // TODO: other devices will be supported in a sdXpY format
    return -1;
}

static int fs_type(Drive *drive, int partition) {
    if(is_ext2(drive, partition)) {
        return FS_TYPE_EXT2;
    }

    return 0;
}

usize load_file(const char *path, void *buffer, usize size) {
    printf("Loading file: %s\n", path);
    Drive *drive;
    int partition;

    int path_offset = device_from_path(path, &drive, &partition);
    if(path_offset < 0) {
        printf("Unsupported device in path: %s\n", path);
        for(;;);
    }

    int type = fs_type(drive, partition);
    if(type == FS_TYPE_EXT2) {
        printf("TODO: ext2 load file\n");
    }

    for(;;);
}
