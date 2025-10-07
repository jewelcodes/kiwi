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

/* local APIC */
#define LAPIC_ID                        0x020
#define LAPIC_VERSION                   0x030
#define LAPIC_TASK_PRIORITY             0x080
#define LAPIC_ARBITRATION_PRIORITY      0x090
#define LAPIC_PROCESSOR_PRIORITY        0x0A0
#define LAPIC_EOI                       0x0B0
#define LAPIC_DESTINATIO                0x0D0
#define LAPIC_DESTINATION_FORMAT        0x0E0
#define LAPIC_SPURIOUS_INTERRUPT        0x0F0
#define LAPIC_ERROR_STATUS              0x280
#define LAPIC_INT_COMMAND_LOW           0x300
#define LAPIC_INT_COMMAND_HIGH          0x310
#define LAPIC_LVT_TIMER                 0x320
#define LAPIC_LVT_LINT0                 0x350
#define LAPIC_LVT_LINT1                 0x360
#define LAPIC_LVT_ERROR                 0x370
#define LAPIC_TIMER_INITIAL_COUNT       0x380
#define LAPIC_TIMER_CURRENT_COUNT       0x390
#define LAPIC_TIMER_DIVIDE_CONFIG       0x3E0

#define LAPIC_LVT_MASK                  0x10000
#define LAPIC_LVT_TRIGGER_LEVEL         0x8000
#define LAPIC_LVT_TRIGGER_LOW           0x2000

#define LAPIC_LVT_SMI                   0x200
#define LAPIC_LVT_NMI                   0x400
#define LAPIC_LVT_EXTINT                0x700
#define LAPIC_LVT_INIT                  0x500

#define LAPIC_TIMER_ONESHOT             0x00000
#define LAPIC_TIMER_PERIODIC            0x20000
#define LAPIC_TIMER_TSC_DEADLINE        0x40000

#define LAPIC_TIMER_VECTOR              0xFE
#define LAPIC_SPURIOUS_VECTOR           0xFF
#define LAPIC_SPURIOUS_ENABLE           0x100

#define LAPIC_TIMER_DIVIDER_2           0x00
#define LAPIC_TIMER_DIVIDER_4           0x01
#define LAPIC_TIMER_DIVIDER_8           0x02
#define LAPIC_TIMER_DIVIDER_16          0x03
#define LAPIC_TIMER_DIVIDER_32          0x08
#define LAPIC_TIMER_DIVIDER_64          0x09
#define LAPIC_TIMER_DIVIDER_128         0x0A
#define LAPIC_TIMER_DIVIDER_1           0x0B

#define LAPIC_INT_COMMAND_INIT          0x500
#define LAPIC_INT_COMMAND_STARTUP       0x600
#define LAPIC_INT_COMMAND_DELIVERED     0x1000 /* set to ZERO on success */
#define LAPIC_INT_COMMAND_LEVEL_ASSERT  0x4000
#define LAPIC_INT_COMMAND_TRIGGER_LEVEL 0x8000

typedef struct LocalAPIC {
    u32 acpi_id;
    u32 apic_id;
    u8 enabled;
    u8 up;
} LocalAPIC;

void apic_init(void);
void lapic_init(uptr mmio_base);
void lapic_write(u32 reg, u32 val);
u32 lapic_read(u32 reg);
void lapic_register(MADTLocalAPIC *entry, int up);
