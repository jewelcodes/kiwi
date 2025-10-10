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
#include <kiwi/tty.h>
#include <kiwi/debug.h>
#include <kiwi/version.h>
#include <kiwi/pmm.h>
#include <kiwi/vmm.h>
#include <kiwi/acpi.h>
#include <kiwi/arch/apic.h>
#include <kiwi/arch/timer.h>
#include <kiwi/arch/smp.h>
#include <string.h>
#include <stdlib.h>

void arch_dt_setup(void);
void arch_exceptions_setup(void);

int arch_early_main(KiwiBootInfo *boot_info_ptr) {
    memcpy(&kiwi_boot_info, boot_info_ptr, sizeof(KiwiBootInfo));

    kernel_terminal.width = kiwi_boot_info.framebuffer_width;
    kernel_terminal.height = kiwi_boot_info.framebuffer_height;
    kernel_terminal.pitch = kiwi_boot_info.framebuffer_pitch;
    kernel_terminal.bpp = kiwi_boot_info.framebuffer_bpp;
    kernel_terminal.front_buffer = (u32 *)kiwi_boot_info.framebuffer;
    kernel_terminal.bg = palette[BLACK];
    kernel_terminal.fg = palette[LIGHT_GRAY];

    tty_clear();

    debug_info(KERNEL_VERSION);
    debug_info("booting with command line: %s", kiwi_boot_info.command_line);
    debug_info("framebuffer @ 0x%08llX: %ux%ux%u, pitch %u",
        (uptr) kernel_terminal.front_buffer,
        kernel_terminal.width,
        kernel_terminal.height,
        kernel_terminal.bpp,
        kernel_terminal.pitch);

    arch_dt_setup();
    arch_exceptions_setup();
    pmm_init();
    vmm_init();

    kernel_terminal.back_buffer = malloc(kernel_terminal.pitch * kernel_terminal.height);
    if(!kernel_terminal.back_buffer) {
        debug_error("failed to allocate back buffer");
        for(;;);
    }

    memcpy(kernel_terminal.back_buffer, kernel_terminal.front_buffer,
        kernel_terminal.pitch * kernel_terminal.height);

    debug_info("allocated back buffer @ 0x%08llX",
        (uptr) kernel_terminal.back_buffer);

    acpi_tables_init();
    apic_init();
    arch_timer_init();
    smp_init();

    char **argv;
    int argc = parse_boot_args(&argv);
    debug_info("parsed %d kernel args", argc);
    for(int i = 0; i < argc; i++) {
        debug_info("  arg[%d]: %s", i, argv[i]);
    }

    for(;;);
}
