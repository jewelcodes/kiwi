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
#include <kiwi/arch/timer.h>
#include <kiwi/arch/x86_64.h>
#include <kiwi/arch/ioport.h>
#include <kiwi/arch/irq.h>
#include <kiwi/arch/mp.h>
#include <kiwi/debug.h>
#include <string.h>

static int isr_installed = 0;

#define CALIBRATION_TIME_MS     50ULL

void lapic_timer_irq_stub(void);

void lapic_timer_init(void) {
    CPUIDRegisters cpuid;
    memset(&cpuid, 0, sizeof(CPUIDRegisters));
    arch_read_cpuid(1, &cpuid);
    u8 apic_id = (cpuid.ebx >> 24) & 0xFF;

    LocalAPIC *lapic = lapic_get_by_apic_id(apic_id);
    if(!lapic) {
        debug_error("failed to find APIC ID %u in local list", apic_id);
        for(;;);
    }

    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
    lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASK | LAPIC_TIMER_ONESHOT);
    lapic_write(LAPIC_TIMER_DIVIDE_CONFIG, LAPIC_TIMER_DIVIDER_1);
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0xFFFFFFFF);

    u64 start = lapic_read(LAPIC_TIMER_CURRENT_COUNT);
    arch_timer_block(CALIBRATION_TIME_MS * 1000000);
    u64 end = lapic_read(LAPIC_TIMER_CURRENT_COUNT);
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);

    u64 ticks = start - end;
    lapic->timer_frequency = (ticks * 1000) / CALIBRATION_TIME_MS;
    debug_info("local APIC ID %u timer @ %llu MHz", lapic->apic_id, lapic->timer_frequency / 1000000);

    u64 initial = lapic->timer_frequency / ARCH_GLOBAL_TIMER_FREQUENCY;
    lapic->timer_ticks = 0;

    if(!isr_installed) {
        isr_installed = 1;
        arch_install_isr(LAPIC_TIMER_VECTOR, (uptr) lapic_timer_irq_stub, GDT_KERNEL_CODE << 3, 0);
    }

    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_VECTOR | LAPIC_TIMER_PERIODIC);
    lapic_write(LAPIC_TIMER_DIVIDE_CONFIG, LAPIC_TIMER_DIVIDER_1);
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, (u32) initial);

    arch_enable_irqs();
}

void lapic_timer_irq(IRQStackFrame *state) {
    int user_transition = (state->cs & 3) != 0;
    if(user_transition) {
        arch_swapgs();
    }

    CPUInfo *cpu = arch_get_current_cpu_info();
    cpu->local_apic->timer_ticks++;

    if(user_transition) {
        arch_swapgs();
    }

    arch_ack_irq(NULL);
}