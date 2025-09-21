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

#pragma once

#include <kiwi/types.h>

    #define VBE_GET_CONTROLLER          0x4F00
    #define VBE_GET_MODE                0x4F01
    #define VBE_SET_MODE                0x4F02
    #define VBE_GET_EDID                0x4F15

    #define VBE_SUCCESS                 0x004F

    #define VBE_MODE_LINEAR             0x4000
    #define MAX_VBE_MODES               32

typedef struct VBEControllerInfo {
    char signature[4];          // "VBE2" on call, "VESA" on return
    u16 version;
    u16 oem_offset;
    u16 oem_segment;
    u32 capabilities;
    u16 mode_offset;
    u16 mode_segment;
    u16 memory;                 // in 64 KB blocks
    u8 reserved[492];
} __attribute__((packed)) VBEControllerInfo;

typedef struct VBEModeInfo {
    u16 attributes;        // we don't care abt anything here other than linearity
    u8 window[2];          // deprecated
    u16 granularity;
    u16 window_size;
    u16 segment[2];
    u32 bank_switch;

    u16 pitch;              // size of one horizontal line in bytes
    u16 width;
    u16 height;
    u8 x_char;              // deprecated
    u8 y_char;
    u8 planes;
    u8 bpp;
    u8 bank_count;
    u8 memory_model;
    u8 bank_size;
    u8 image_pages;
    u8 reserved0;

    u8 red_mask;
    u8 red_position;
    u8 green_mask;
    u8 green_position;
    u8 blue_mask;
    u8 blue_position;
    u8 reserved_mask;
    u8 reserved_position;
    u8 direct_color_attributes;

    u32 framebuffer;       // physical address
    u32 off_screen_buffer;
    u16 off_screen_size;

    u8 reserved1[206];
} __attribute__((packed)) VBEModeInfo;

typedef struct EDIDTiming {
    u8 h_frequency;
    u8 v_frequency;
    u8 h_active_low;
    u8 h_blank_low;
    u8 h_active_blank_high;
    u8 v_active_low;
    u8 v_blank_low;
    u8 v_active_blank_high;

    u8 h_sync;
    u8 h_sync_pulse;
    u8 v_sync;
    u8 v_sync_pulse;

    u8 h_size_mm;
    u8 v_size_mm;
    u8 aspect_ratio;
    u8 h_border;
    u8 v_border;

    u8 display_type;
} __attribute__((packed)) EDIDTiming;

typedef struct EDIDDisplay {
    u8 padding[8];
    u16 manufacturer;
    u16 id;
    u32 serial;
    u8 manufacture_week;
    u8 manufacture_year;

    u8 version;
    u8 revision;

    u8 input_type;
    u8 horizontal_size_cm;
    u8 vertical_size_cm;
    u8 gamma_factor;
    u8 DPM_flags;
    u8 chroma[10];

    u8 est_timing1;
    u8 est_timing2;
    u8 res_timing1;
    u16 std_timing[8];

    EDIDTiming timing[4];

    u8 reserved;
    u8 checksum;
} __attribute__((packed)) EDIDDisplay;

typedef struct VideoMode {
    u16 width, height, mode_number;
    u8 bpp;
    u32 framebuffer, pitch;
    char label[32];
} VideoMode;

extern VideoMode video_modes[];
int vbe_init(void);
VideoMode *vbe_set_mode(u16 width, u16 height, u8 bpp);
void vbe_configure(void);
