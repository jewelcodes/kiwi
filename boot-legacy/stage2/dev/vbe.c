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
#include <boot/vbe.h>
#include <stdio.h>
#include <string.h>

#define MIN_WIDTH               800
#define MIN_HEIGHT              600
#define MIN_BPP                 32

VideoMode video_modes[MAX_VBE_MODES];

static int video_mode_count;
static Registers vbe_regs;
static VBEControllerInfo controller_info;
static VBEModeInfo mode_info;

static int get_mode_info(u16 mode_number, VBEModeInfo *info);

int vbe_init(void) {
    memset(&controller_info, 0, sizeof(VBEControllerInfo));
    memcpy(controller_info.signature, "VBE2", 4);
    controller_info.version = 0x0300;

    vbe_regs.eax = VBE_GET_CONTROLLER;
    vbe_regs.es = ((u32) &controller_info >> 4) & 0xFFFF;
    vbe_regs.edi = ((u32) &controller_info) & 0x0F;
    bios_int(0x10, &vbe_regs);

    if(((vbe_regs.eax & 0xFFFF) != VBE_SUCCESS)
        || memcmp(controller_info.signature, "VESA", 4)) {
        printf("vbe: failed to get controller info; status code = 0x%04X\n",
            vbe_regs.eax & 0xFFFF);
        for(;;);
    }

    if(controller_info.version < 0x0200) {
        printf("vbe: VBE version 2.0 or higher is required\n");
        for(;;);
    }

    video_mode_count = 0;
    u16 *modes = (u16 *) ((controller_info.mode_segment << 4)
        + controller_info.mode_offset);
    
    for(; *modes != 0xFFFF && video_mode_count < MAX_VBE_MODES; modes++) {
        get_mode_info(*modes, &mode_info);
    }

    for(;;);
}

static int get_mode_info(u16 mode_number, VBEModeInfo *info) {
    memset(info, 0, sizeof(VBEModeInfo));

    vbe_regs.eax = VBE_GET_MODE;
    vbe_regs.ecx = mode_number;
    vbe_regs.es = ((u32) info >> 4) & 0xFFFF;
    vbe_regs.edi = ((u32) info) & 0x0F;
    bios_int(0x10, &vbe_regs);

    if((vbe_regs.eax & 0xFFFF) != VBE_SUCCESS) {
        printf("vbe: failed to get mode info for mode 0x%04X; status code = 0x%04X\n",
            mode_number, vbe_regs.eax & 0xFFFF);
        return -1;
    }

    if(info->width >= MIN_WIDTH && info->height >= MIN_HEIGHT
        && info->bpp >= MIN_BPP) {
        VideoMode *mode = &video_modes[video_mode_count++];
        mode->width = info->width;
        mode->height = info->height;
        mode->bpp = info->bpp;
        mode->mode_number = mode_number;
        mode->framebuffer = info->framebuffer;
        mode->pitch = info->pitch;

        printf("vbe: found mode 0x%04X: %dx%d@%d, framebuffer=0x%08X, pitch=%d\n",
            mode_number, mode->width, mode->height, mode->bpp,
            mode->framebuffer, mode->pitch);
    }

    return 0;
}
