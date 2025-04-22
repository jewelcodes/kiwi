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

    mountpoint->superblock = malloc(512*1024); // max block size
    if(!mountpoint->superblock) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for superblock\n");
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    printf(ESC_BOLD_CYAN "mount:" ESC_RESET " mounting disk image %s\n", argv[1]);

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

    /* open the disk image */
    mountpoint->disk = fopen(argv[1], "rb+");
    if(!mountpoint->disk) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to open disk image %s\n", argv[1]);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    /* search for the superblock according to the block sizes */
    mountpoint->block_size = 4096; // smallest block size
    while(mountpoint->block_size <= 512*1024) {
        if(!read_block(mountpoint->disk, SUPERBLOCK_BLOCK_NUMBER,
            mountpoint->block_size, 1, mountpoint->superblock)) {
            
            u8 *magic = (u8 *)&mountpoint->superblock->magic;

            if((!memcmp(&mountpoint->superblock->magic, SUPER_MAGIC_STRING, 7) &&
                magic[7] == SUPER_MAGIC_VERSION) &&
                mountpoint->superblock->major_revision == SUPER_MAJOR_REVISION &&
                mountpoint->superblock->minor_revision == SUPER_MINOR_REVISION &&
                mountpoint->superblock->patch == SUPER_PATCH_REVISION) {

                break;
            }
        } else {
            printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to read superblock on %s\n", argv[1]);
            fclose(mountpoint->disk);
            free(mountpoint);
            mountpoint = NULL;
            return 1;
        }

        mountpoint->block_size *= 2;
    }

    if(mountpoint->block_size > 512*1024) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to find superblock in %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    u64 checksum = mountpoint->superblock->checksum;
    mountpoint->superblock->checksum = 0;
    u64 calculated = xxhash64(mountpoint->superblock, mountpoint->superblock->superblock_size);
    if(calculated != checksum) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " invalid superblock checksum on %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    // allocate memory
    mountpoint->data_block = malloc(mountpoint->block_size);
    if(!mountpoint->data_block) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for disk image %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    mountpoint->metadata_block = malloc(mountpoint->block_size);
    if(!mountpoint->metadata_block) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for disk image %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint->data_block);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    mountpoint->bitmap_block = malloc(mountpoint->block_size);
    if(!mountpoint->bitmap_block) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for disk image %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint->data_block);
        free(mountpoint->metadata_block);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    mountpoint->highest_layer_bitmap = malloc(mountpoint->block_size);
    if(!mountpoint->highest_layer_bitmap) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for disk image %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint->data_block);
        free(mountpoint->metadata_block);
        free(mountpoint->bitmap_block);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    // find the fanout and bitmap depth
    u16 bitmap_limit;

    switch(mountpoint->superblock->tuning & SUPER_TUNING_FANOUT_FACTOR_MASK) {
    case SUPER_TUNING_FANOUT_FACTOR_8:
        mountpoint->fanout = 8;
        break;
    case SUPER_TUNING_FANOUT_FACTOR_16:
        mountpoint->fanout = 16;
        break;
    case SUPER_TUNING_FANOUT_FACTOR_32:
        mountpoint->fanout = 32;
        break;
    case SUPER_TUNING_FANOUT_FACTOR_64:
        mountpoint->fanout = 64;
        break;
    default:
        printf(ESC_BOLD_RED "mount:" ESC_RESET " invalid fanout factor on %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    switch(mountpoint->superblock->tuning & SUPER_TUNING_BITMAP_LIMIT_MASK) {
    case SUPER_TUNING_BITMAP_LIMIT_4096:
        bitmap_limit = 4096;
        break;
    case SUPER_TUNING_BITMAP_LIMIT_8192:
        bitmap_limit = 8192;
        break;
    case SUPER_TUNING_BITMAP_LIMIT_16384:
        bitmap_limit = 16384;
        break;
    case SUPER_TUNING_BITMAP_LIMIT_32768:
        bitmap_limit = 32768;
        break;
    default:
        printf(ESC_BOLD_RED "mount:" ESC_RESET " invalid bitmap limit on %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    u64 bitmap_size_bits = mountpoint->superblock->volume_size;
    u64 highest_layer = bitmap_size_bits;

    mountpoint->bitmap_layers = 1;

    while(highest_layer > bitmap_limit) {
        highest_layer /= mountpoint->fanout;
        bitmap_size_bits += highest_layer;
        mountpoint->bitmap_layers++;
    }

    mountpoint->highest_layer_size = highest_layer;

    // cache the highest layer bitmap
    if(read_block(mountpoint->disk, mountpoint->superblock->bitmap_block,
        mountpoint->block_size, 1, mountpoint->highest_layer_bitmap)) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to read bitmap on %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint->data_block);
        free(mountpoint->metadata_block);
        free(mountpoint->bitmap_block);
        free(mountpoint->highest_layer_bitmap);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    // and the starting offset of each layer
    mountpoint->layer_starts = calloc(mountpoint->bitmap_layers, sizeof(u64));
    if(!mountpoint->layer_starts) {
        printf(ESC_BOLD_RED "mount:" ESC_RESET " failed to allocate memory for disk image %s\n", argv[1]);
        fclose(mountpoint->disk);
        free(mountpoint->data_block);
        free(mountpoint->metadata_block);
        free(mountpoint->bitmap_block);
        free(mountpoint->highest_layer_bitmap);
        free(mountpoint);
        mountpoint = NULL;
        return 1;
    }

    for(int i = mountpoint->bitmap_layers-1; i >= 0; i--) {
        if(i == mountpoint->bitmap_layers-1) {
            mountpoint->layer_starts[i] = 0;
        } else if(i == mountpoint->bitmap_layers-2) {
            mountpoint->layer_starts[i] = mountpoint->highest_layer_size;
        } else {
            mountpoint->layer_starts[i] = mountpoint->layer_starts[i+1] * mountpoint->fanout;
        }
    }

    printf(ESC_BOLD_GREEN "mount:" ESC_RESET " ✅ mounted disk image %s\n", argv[1]);
    return 0;
}
