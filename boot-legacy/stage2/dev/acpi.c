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

#include <boot/acpi.h>
#include <stdio.h>
#include <string.h>

ACPIRSDP *rsdp = NULL;

int acpi_init(void) {
    u16 *ebda_segment_ptr = (u16 *)0x40E;
    u32 ebda_ptr = (*ebda_segment_ptr) << 4;

    for(int i = 0; i < 1024; i += 16) {
        if(!memcmp((void *)(ebda_ptr + i), "RSD PTR ", 8)) {
            rsdp = (ACPIRSDP *)(ebda_ptr + i);
            break;
        }
    }

    if(rsdp) {
        goto found;
    }

    for(u32 addr = 0xE0000; addr < 0x100000; addr += 16) {
        if(!memcmp((void *)addr, "RSD PTR ", 8)) {
            rsdp = (ACPIRSDP *)addr;
            break;
        }
    }

    if(!rsdp) {
        printf("System is not ACPI compliant.\n");
        for(;;);
    }

found:

    int size = (!rsdp->revision) ? 20 : rsdp->length;
    u8 sum = 0;

    for(int i = 0; i < size; i++) {
        sum += *((u8 *)rsdp + i);
    }

    if(sum != 0) {
        printf("RSDP checksum is invalid.\n");
        for(;;);
    }

    return 0;
}