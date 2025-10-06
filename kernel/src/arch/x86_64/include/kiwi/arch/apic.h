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
#include <kiwi/acpi.h>

typedef struct ACPIMADT {
    ACPIHeader header;
    u32 lapic_mmio_base;
    u32 flags;

    u8 entries[];
} __attribute__((packed)) ACPIMADT;

#define MADT_FLAGS_LEGACY_PIC           0x01

typedef struct MADTEntryHeader {
    u8 type;
    u8 length;
} __attribute__((packed)) MADTEntryHeader;

#define MADT_ENTRY_TYPE_LAPIC           0x00
#define MADT_ENTRY_TYPE_IOAPIC          0x01
#define MADT_ENTRY_TYPE_OVERRIDE        0x02
#define MADT_ENTRY_TYPE_IOAPIC_NMI      0x03
#define MADT_ENTRY_TYPE_LAPIC_NMI       0x04
#define MADT_ENTRY_TYPE_LAPIC_OVERRIDE  0x05

#define MADT_TRIGGER_MODE_ACTIVE_LOW    0x02
#define MADT_TRIGGER_MODE_LEVEL         0x08

typedef struct MADTLocalAPIC {
    MADTEntryHeader header;
    u8 acpi_id;
    u8 apic_id;
    u32 flags;
} __attribute__((packed)) MADTLocalAPIC;

#define MADT_LAPIC_FLAGS_ENABLED        0x01

typedef struct MADTIOAPIC {
    MADTEntryHeader header;
    u8 ioapic_id;
    u8 reserved;
    u32 mmio_base;
    u32 gsi_base;
} __attribute__((packed)) MADTIOAPIC;

typedef struct MADTInterruptOverride {
    MADTEntryHeader header;
    u8 bus_source;
    u8 irq_source;
    u32 gsi;
    u16 flags;
} __attribute__((packed)) MADTInterruptOverride;

typedef struct MADTIOAPICNMI {
    MADTEntryHeader header;
    u8 ioapic_id;
    u8 reserved;
    u32 gsi;
    u16 flags;
} __attribute__((packed)) MADTIOAPICNMI;

typedef struct MADTLocalAPICNMI {
    MADTEntryHeader header;
    u8 acpi_id;
    u16 flags;
    u8 lint;
} __attribute__((packed)) MADTLocalAPICNMI;

typedef struct MADTLocalAPICOverride {
    MADTEntryHeader header;
    u16 reserved;
    u64 mmio_base;
} __attribute__((packed)) MADTLocalAPICOverride;

void apic_init(void);
