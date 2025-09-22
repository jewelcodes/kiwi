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

#include <boot/vbe.h>
#include <boot/memory.h>
#include <boot/menu.h>
#include <boot/disk.h>
#include <stdio.h>

static void about(void) {
    dialog("About Kiwi",
        "Kiwi is a prototype high-performance general-purpose\n"
        "operating system built entirely from scratch.\n\n"
        "Kiwi is free and open-source software released under the\n"
        "MIT License. Visit https://github.com/jewelcodes/kiwi\n"
        "for more info and source code.",
        56, 11);
}

static void sysinfo(void) {
    char buffer[4096];

    snprintf(buffer, sizeof(buffer),
        "Total memory: %llu KB (%llu MB)\n"
        "Usable memory: %llu KB (%llu MB)\n"
        "Hardware-reserved memory: %llu KB (%llu MB)\n\n"
        "VESA BIOS OEM: %s\n"
        "Video memory: %u KB (%u MB)",
        total_memory / 1024, total_memory / (1024 * 1024),
        total_usable_memory / 1024, total_usable_memory / (1024 * 1024),
        (total_memory - total_usable_memory) / 1024,
        (total_memory - total_usable_memory) / (1024 * 1024),
        video_controller, video_memory / 1024, video_memory / (1024 * 1024));

    dialog("System Information", buffer, 58, 11);
}

int main(void) {
    vbe_init();
    detect_memory();
    disk_init();

    MenuState menu;

    const char *items[] = {
        "Kiwi (normal boot)", // 0
        "Kiwi (debug)",       // 1
        NULL,                 // 2 - separator
        "Configure display",  // 3
        "System information", // 4
        "About Kiwi",         // 5
    };

    menu.title = "Kiwi Boot Manager";
    menu.items = items;
    menu.count = sizeof(items) / sizeof(items[0]);
    menu.selected = 0;
    menu.top_visible_index = 0;

    for(;;) {
        int selection = drive_menu(&menu, 0);
        if(selection == 3) vbe_configure();
        else if(selection == 4) sysinfo();
        else if(selection == 5) about();
    }

    for(;;);
}
