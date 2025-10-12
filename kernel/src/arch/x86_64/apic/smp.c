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

#include <kiwi/structs/array.h>
#include <kiwi/arch/x86_64.h>
#include <kiwi/arch/apic.h>
#include <kiwi/arch/smp.h>
#include <kiwi/arch/mp.h>
#include <kiwi/arch/paging.h>
#include <kiwi/debug.h>
#include <kiwi/pmm.h>
#include <kiwi/vmm.h>
#include <string.h>
#include <stdlib.h>

#define AP_INITIAL_STACK_PAGES  8                   /* 32 KB */
#define IRQ_STACK_SIZE          (16 * PAGE_SIZE)    /* 64 KB */
#define USER_STACK_SIZE         (16 * PAGE_SIZE)    /* 64 KB */

#define AP_ENTRY_POINT          0x1000
#define CR3_PTR                 0x2000
#define STACK_PTR               0x2008
#define ENTRY_POINT_PTR         0x2010

extern u8 ap_early_main[];
extern u8 ap_early_main_end[];

static int booted = 0;
static Array *cpu_infos;

int arch_get_cpu_count(void) {
    return (int) cpu_infos->count;
}

CPUInfo *arch_get_cpu_info(int index) {
    if((index < 0) || (index >= cpu_infos->count)) {
        return NULL;
    }

    return (CPUInfo *) cpu_infos->items[index];
}

static void smp_cpu_info_init(LocalAPIC *lapic) {
    CPUIDRegisters cpuid;
    memset(&cpuid, 0, sizeof(CPUIDRegisters));
    arch_read_cpuid(7, &cpuid);
    if(cpuid.ebx & 1) { // rdfsbase, rdgsbase
        arch_set_cr4(arch_get_cr4() | CR4_FSGSBASE);
    }

    memset(&cpuid, 0, sizeof(CPUIDRegisters));
    arch_read_cpuid(0x80000001, &cpuid);

    if(!(cpuid.edx & (1 << 11))) { // fast syscall/sysret
        debug_error("CPU doesn't support SYSCALL/SYSRET");
        for(;;);
    }

    arch_write_msr(MSR_EFER, arch_read_msr(MSR_EFER) | MSR_EFER_SYSCALL);

    if(cpuid.edx & (1 << 25)) { // fast fxsave/restore
        arch_write_msr(MSR_EFER, arch_read_msr(MSR_EFER) | MSR_EFER_FFXSR);
    }

    GDTEntry *new_gdt = (GDTEntry *) calloc(GDT_ENTRIES, sizeof(GDTEntry));
    if(!new_gdt) {
        goto no_memory;
    }

    memcpy(new_gdt, gdt, sizeof(GDTEntry) * GDT_ENTRIES);

    TSS *tss = (TSS *) calloc(1, sizeof(TSS));
    if(!tss) {
        goto no_memory;
    }

    void *irq_stack = calloc(1, IRQ_STACK_SIZE);
    if(!irq_stack) {
        goto no_memory;
    }

    void *user_stack = calloc(1, USER_STACK_SIZE);
    if(!user_stack) {
        goto no_memory;
    }

    tss->rsp0 = (u64) irq_stack + IRQ_STACK_SIZE;
    tss->ist[0] = tss->rsp0;
    tss->iomap_offset = 0x68;
    memset(tss->iomap, 0xFF, sizeof(tss->iomap));
    tss->ones = 0xFF;

    new_gdt[GDT_TSS_LOW].base_low = (uptr) tss;
    new_gdt[GDT_TSS_LOW].base_middle = ((uptr) tss >> 16);
    new_gdt[GDT_TSS_LOW].base_high = ((uptr) tss >> 24);
    new_gdt[GDT_TSS_LOW].limit_low = sizeof(TSS) - 1;
    new_gdt[GDT_TSS_LOW].access = GDT_ACCESS_TSS | GDT_ACCESS_PRESENT;

    u64 *high = (u64 *) &new_gdt[GDT_TSS_HIGH];
    *high = (uptr) tss >> 32;

    GDTR gdtr;
    gdtr.limit = (sizeof(GDTEntry) * GDT_ENTRIES) - 1;
    gdtr.base = (uptr) new_gdt;
    arch_load_gdt(&gdtr);
    arch_reload_code_segment(GDT_KERNEL_CODE << 3);
    arch_reload_data_segments(GDT_KERNEL_DATA << 3);
    arch_load_tss(GDT_TSS_LOW << 3);

    CPUInfo *cpu_info = (CPUInfo *) calloc(1, sizeof(CPUInfo));
    if(!cpu_info) {
        debug_error("failed to allocate CPU info");
        for(;;);
    }

    cpu_info->cpu_info = cpu_info;
    cpu_info->stack = (void *) ((uptr) user_stack + USER_STACK_SIZE);
    cpu_info->local_apic = lapic;
    cpu_info->index = (int) cpu_infos->count;

    if(array_push(cpu_infos, (uptr) cpu_info)) {
        goto no_memory;
    }

    // this state looks inverted but it is actually correct because we are in
    // kernel mode now and so the actual base used is MSR_GS_BASE.
    // before switching to user mode, we will run swapgs

    arch_write_msr(MSR_KERNEL_GS_BASE, 0);
    arch_write_msr(MSR_GS_BASE, (u64) cpu_info);
    arch_write_msr(MSR_FS_BASE, 0);

    lapic->up = 1;
    return;

no_memory:
    debug_error("failed to allocate memory for CPU info");
    for(;;);
}

void ap_main(void) {
    IDTR idtr;
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (u64)&idt;
    arch_load_idt(&idtr);

    CPUIDRegisters cpuid;
    memset(&cpuid, 0, sizeof(CPUIDRegisters));
    arch_read_cpuid(1, &cpuid);
    u8 apic_id = (cpuid.ebx >> 24) & 0xFF;

    LocalAPIC *lapic = lapic_get_by_apic_id(apic_id);
    if(!lapic) {
        debug_error("failed to find AP %u in LAPIC list", apic_id);
        for(;;);
    }

    lapic_init(0);
    smp_cpu_info_init(lapic);
    lapic_timer_init();

    booted = 1;
    arch_flush_cache();
    for(;;);
}

void smp_init(void) {
    cpu_infos = array_create();
    if(!cpu_infos) {
        debug_error("failed to allocate CPU info array");
        for(;;);
    }

    CPUIDRegisters cpuid;
    memset(&cpuid, 0, sizeof(CPUIDRegisters));
    arch_read_cpuid(1, &cpuid);
    u8 bsp_id = (cpuid.ebx >> 24) & 0xFF;

    LocalAPIC *bsp = lapic_get_by_apic_id(bsp_id);
    if(!bsp) {
        debug_error("failed to find BSP in LAPIC list");
        for(;;);
    }

    smp_cpu_info_init(bsp);
    lapic_timer_init();

    if(lapics->count < 2) {
        return;
    }

    debug_info("attempt to start %u secondary cores...", lapics->count - 1);

    // temporarily map low memory so the AP can access it
    for(int i = 0; i < 8; i++) {
        arch_map_page(arch_get_cr3(), i * PAGE_SIZE, i * PAGE_SIZE,
            VMM_PROT_READ | VMM_PROT_WRITE | VMM_PROT_EXEC);
    }

    u64 volatile *cr3_ptr = (u64 volatile *) CR3_PTR;
    *cr3_ptr = arch_get_cr3();

    u64 volatile *stack_ptr = (u64 volatile *) STACK_PTR;
    u64 volatile *entry_point_ptr = (u64 volatile *) ENTRY_POINT_PTR;

    usize ap_code_size = (usize)((uptr) &ap_early_main_end - (uptr) &ap_early_main);

    for(int i = 0; i < lapics->count; i++) {
        LocalAPIC *lapic = (LocalAPIC *) lapics->items[i];
        if(!lapic || !lapic->enabled || lapic->up) {
            continue;
        }

        debug_info("booting CPU with APIC ID %u...", lapic->apic_id);

        void *new_stack = calloc(AP_INITIAL_STACK_PAGES, PAGE_SIZE);
        if(!new_stack) {
            debug_error("failed to allocate stack");
            for(;;);
        }

        *stack_ptr = (u64) new_stack + (AP_INITIAL_STACK_PAGES * PAGE_SIZE);
        *entry_point_ptr = (u64) &ap_main;

        memcpy((void *) AP_ENTRY_POINT + ARCH_HHDM_BASE, &ap_early_main, ap_code_size);
        booted = 0;
        arch_flush_cache();

        lapic_write(LAPIC_INT_COMMAND_HIGH, lapic->apic_id << 24);
        lapic_write(LAPIC_INT_COMMAND_LOW, LAPIC_INT_COMMAND_INIT
            | LAPIC_INT_COMMAND_LEVEL_ASSERT | LAPIC_INT_COMMAND_TRIGGER_LEVEL);
        while(lapic_read(LAPIC_INT_COMMAND_LOW) & LAPIC_INT_COMMAND_DELIVERED);

        lapic_write(LAPIC_INT_COMMAND_HIGH, lapic->apic_id << 24);
        lapic_write(LAPIC_INT_COMMAND_LOW, LAPIC_INT_COMMAND_INIT);
        while(lapic_read(LAPIC_INT_COMMAND_LOW) & LAPIC_INT_COMMAND_DELIVERED);

        lapic_write(LAPIC_INT_COMMAND_HIGH, lapic->apic_id << 24);
        lapic_write(LAPIC_INT_COMMAND_LOW, LAPIC_INT_COMMAND_STARTUP
            | LAPIC_INT_COMMAND_TRIGGER_LEVEL | (AP_ENTRY_POINT >> 12));

        while(!booted) {
            arch_spin_backoff();
        }
    }

    for(int i = 0; i < 8; i++) {
        arch_unmap_page(arch_get_cr3(), i * PAGE_SIZE);
    }
}
