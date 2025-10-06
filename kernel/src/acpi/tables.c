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
#include <kiwi/acpi.h>
#include <kiwi/debug.h>
#include <kiwi/vmm.h>
#include <string.h>

static ACPIRSDP *rsdp;
static ACPIRSDT *rsdt;
static ACPIXSDT *xsdt;

static void acpi_table_summary(ACPIHeader *header) {
    debug_info("'%c%c%c%c' v%u @ 0x%llX, len %u, OEM ID '%c%c%c%c%c%c'",
        header->signature[0],
        header->signature[1],
        header->signature[2],
        header->signature[3],
        header->revision,
        (uptr) header - ARCH_HHDM_BASE,
        header->length,
        header->oem_id[0],
        header->oem_id[1],
        header->oem_id[2],
        header->oem_id[3],
        header->oem_id[4],
        header->oem_id[5]);
}

void acpi_tables_init(void) {
    if(!kiwi_boot_info.acpi_rsdp) {
        debug_error("system does not support ACPI");
        for(;;);
    }

    rsdp = (ACPIRSDP *) ((uptr) kiwi_boot_info.acpi_rsdp + ARCH_HHDM_BASE);
    debug_info("'RSD PTR ' revision %u @ 0x%llX, OEM ID '%c%c%c%c%c%c'",
        rsdp->revision,
        kiwi_boot_info.acpi_rsdp,
        rsdp->oem_id[0],
        rsdp->oem_id[1],
        rsdp->oem_id[2],
        rsdp->oem_id[3],
        rsdp->oem_id[4],
        rsdp->oem_id[5]);
    
    if(rsdp->revision) {
        rsdt = NULL;
        xsdt = (ACPIXSDT *) ((uptr) rsdp->xsdt + ARCH_HHDM_BASE);
        acpi_table_summary(&xsdt->header);
    } else {
        xsdt = NULL;
        rsdt = (ACPIRSDT *) ((uptr) rsdp->rsdt + ARCH_HHDM_BASE);
        acpi_table_summary(&rsdt->header);
    }

    int table_count = xsdt
        ? (xsdt->header.length - sizeof(ACPIHeader)) / sizeof(u64)
        : (rsdt->header.length - sizeof(ACPIHeader)) / sizeof(u32);
    
    for(int i = 0; i < table_count; i++) {
        ACPIHeader *header = xsdt
            ? (ACPIHeader *) ((uptr) xsdt->entries[i] + ARCH_HHDM_BASE)
            : (ACPIHeader *) ((uptr) rsdt->entries[i] + ARCH_HHDM_BASE);
        acpi_table_summary(header);
    }
}

void *acpi_find_table(const char *signature, usize index) {
    int table_count = xsdt
        ? (xsdt->header.length - sizeof(ACPIHeader)) / sizeof(u64)
        : (rsdt->header.length - sizeof(ACPIHeader)) / sizeof(u32);
    
    for(int i = 0; i < table_count; i++) {
        ACPIHeader *header = xsdt
            ? (ACPIHeader *) ((uptr) xsdt->entries[i] + ARCH_HHDM_BASE)
            : (ACPIHeader *) ((uptr) rsdt->entries[i] + ARCH_HHDM_BASE);

        if((!memcmp(header->signature, signature, 4))) {
            if(index == 0) {
                return header;
            }
            index--;
        }
    }

    return NULL;
}
