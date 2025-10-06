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

#include <kiwi/arch/apic.h>
#include <kiwi/arch/ioport.h>
#include <kiwi/debug.h>
#include <kiwi/acpi.h>

static ACPIMADT *madt;

void apic_init(void) {
    madt = acpi_find_table("APIC", 0);
    if(!madt) {
        debug_error("ACPI MADT table not present");
        for(;;);
    }

    debug_info("local APIC @ 0x%X", madt->lapic_mmio_base);

    if(madt->flags & MADT_FLAGS_LEGACY_PIC) {
        debug_info("disabling legacy PIC...");
        arch_outport8(0x21, 0xFF);
        arch_outport8(0xA1, 0xFF);
    }

    MADTEntryHeader *entry = (MADTEntryHeader *) madt->entries;
    while(((uptr) entry) < ((uptr) madt + madt->header.length)) {
        switch(entry->type) {
            case MADT_ENTRY_TYPE_LAPIC: {
                MADTLocalAPIC *lapic = (MADTLocalAPIC *) entry;
                debug_info("local APIC ID %u, ACPI ID %u, flags 0x%X (%s)",
                    lapic->apic_id,
                    lapic->acpi_id,
                    lapic->flags,
                    (lapic->flags & MADT_LAPIC_FLAGS_ENABLED) ? "enabled" : "disabled");
                break;
            }

            case MADT_ENTRY_TYPE_IOAPIC: {
                MADTIOAPIC *ioapic = (MADTIOAPIC *) entry;
                debug_info("I/O APIC ID %u @ 0x%X, GSI base %u",
                    ioapic->ioapic_id,
                    ioapic->mmio_base,
                    ioapic->gsi_base);
                break;
            }

            case MADT_ENTRY_TYPE_OVERRIDE: {
                MADTInterruptOverride *override = (MADTInterruptOverride *) entry;
                debug_info("override IRQ %u -> GSI %u, flags 0x%X (%s %s)",
                    override->irq_source,
                    override->gsi,
                    override->flags,
                    (override->flags & MADT_TRIGGER_MODE_LEVEL) ? "level" : "edge",
                    (override->flags & MADT_TRIGGER_MODE_ACTIVE_LOW) ? "low" : "high");
                break;
            }

            case MADT_ENTRY_TYPE_LAPIC_NMI: {
                MADTLocalAPICNMI *lapic_nmi = (MADTLocalAPICNMI *) entry;
                debug_info("local NMI, ACPI ID %u, LINT#%u, flags 0x%X (%s %s)",
                    lapic_nmi->acpi_id,
                    lapic_nmi->lint,
                    lapic_nmi->flags,
                    (lapic_nmi->flags & MADT_TRIGGER_MODE_LEVEL) ? "level" : "edge",
                    (lapic_nmi->flags & MADT_TRIGGER_MODE_ACTIVE_LOW) ? "low" : "high");
                break;
            }

            default:
                debug_warn("unknown MADT entry type: %u with size %u", entry->type, entry->length);
                break;
        }

        entry = (MADTEntryHeader *) ((uptr) entry + entry->length);
    }

    for(;;);
}