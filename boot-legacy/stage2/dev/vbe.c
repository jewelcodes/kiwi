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
#include <boot/menu.h>
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
static EDIDDisplay edid_info;

static int get_mode_info(u16 mode_number, VBEModeInfo *info);
static int get_edid(EDIDDisplay *edid);

static u16 preferred_width = 0;
static u16 preferred_height = 0;

static u16 fallback_modes[][2] = {
    {1920, 1080},
    {1600, 900},
    {1366, 768},
    {1360, 768},
    {1280, 720},
    {1024, 768},
    {800, 600}
};

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

    get_edid(&edid_info);

    if(preferred_width && preferred_height) {
        if(!vbe_set_mode(preferred_width, preferred_height, 32)) {
            printf("vbe: failed to set preferred mode %dx%d, trying fallbacks\n",
                preferred_width, preferred_height);
        } else {
            return 0;
        }
    }

    for(int i = 0; i < sizeof(fallback_modes)/sizeof(fallback_modes[0]); i++) {
        if(vbe_set_mode(fallback_modes[i][0], fallback_modes[i][1], 32)) {
            return 0;
        }
    }

    printf("vbe: failed to set any suitable video mode, bailing...\n");
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
        snprintf(mode->label, sizeof(mode->label), "%dx%dx%d",
            mode->width, mode->height, mode->bpp);

        printf("vbe: found mode 0x%04X: %dx%dx%d, framebuffer=0x%08X, pitch=%d\n",
            mode_number, mode->width, mode->height, mode->bpp,
            mode->framebuffer, mode->pitch);
    }

    return 0;
}

static int get_edid(EDIDDisplay *edid) {
    memset(edid, 0, sizeof(EDIDDisplay));

    vbe_regs.eax = VBE_GET_EDID;
    vbe_regs.ebx = 1;
    vbe_regs.ecx = 0;
    vbe_regs.edx = 0;
    vbe_regs.es = ((u32) edid >> 4) & 0xFFFF;
    vbe_regs.edi = ((u32) edid) & 0x0F;
    bios_int(0x10, &vbe_regs);

    if((vbe_regs.eax & 0xFFFF) != VBE_SUCCESS) {
        printf("vbe: failed to get EDID; status code = 0x%04X\n",
            vbe_regs.eax & 0xFFFF);
        return -1;
    }

    for(int i = 0; i < 4; i++) {
        u16 width = edid->timing[i].h_active_low
            | ((edid->timing[i].h_active_blank_high & 0xF0) << 4);
        u16 height = edid->timing[i].v_active_low
            | ((edid->timing[i].v_active_blank_high & 0xF0) << 4);
        
        if(width && height && width >= preferred_width && height >= preferred_height) {
            preferred_width = width;
            preferred_height = height;
        }
    }

    printf("vbe: display preferred resolution = %dx%d\n",
        preferred_width, preferred_height);

    return 0;
}

VideoMode *vbe_set_mode(u16 width, u16 height, u8 bpp) {
    VideoMode *mode = NULL;

    for(int i = 0; i < video_mode_count; i++) {
        if(video_modes[i].width == width
            && video_modes[i].height == height
            && video_modes[i].bpp == bpp) {
            mode = &video_modes[i];
            break;
        }
    }

    if(!mode) {
        printf("vbe: requested mode %dx%dx%d not found\n", width, height, bpp);
        return NULL;
    }

    vbe_regs.eax = VBE_SET_MODE;
    vbe_regs.ebx = mode->mode_number | VBE_MODE_LINEAR;
    vbe_regs.edi = 0;
    bios_int(0x10, &vbe_regs);

    if((vbe_regs.eax & 0xFFFF) != VBE_SUCCESS) {
        printf("vbe: failed to set mode %dx%dx%d (0x%04X); status code = 0x%04X\n",
            mode->width, mode->height, mode->bpp, mode->mode_number,
            vbe_regs.eax & 0xFFFF);
        return NULL;
    }

    display.current_mode = mode;
    display.vbe_enabled = 1;
    display.x = 0;
    display.y = 0;

    display.bg = palette[0]; // black
    display.fg = palette[15]; // white

    clear_screen();
    return mode;
}


void vbe_configure(void) {
    MenuState menu;

    const char *items[MAX_VBE_MODES];
    for(int i = 0; i < video_mode_count; i++) {
        items[i] = video_modes[i].label;
    }

    menu.title = "Kiwi Boot Manager - Select Video Mode";
    menu.items = items;
    menu.count = video_mode_count;

    for(int i = 0; i < video_mode_count; i++) {
        if(video_modes[i].width == display.current_mode->width
            && video_modes[i].height == display.current_mode->height
            && video_modes[i].bpp == display.current_mode->bpp) {
            menu.selected = i;
            break;
        }
    }

    if(menu.count > MAX_VISIBLE_ROWS) {
        menu.top_visible_index = menu.selected - MAX_VISIBLE_ROWS / 2;
        if(menu.top_visible_index < 0) menu.top_visible_index = 0;
    } else {
        menu.top_visible_index = 0;
    }

    for(;;) {
        int selection = drive_menu(&menu, 1);
        if(selection >= 0 && selection < video_mode_count) {
            vbe_set_mode(video_modes[selection].width,
                video_modes[selection].height,
                video_modes[selection].bpp);
        }

        if(selection == -1) {
            // user pressed escape
            return;
        }
    }
}
