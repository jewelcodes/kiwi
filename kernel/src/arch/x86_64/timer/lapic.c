/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2026 Omar Elghoul
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

#include <kiwi/arch/atomic.h>
#include <kiwi/arch/apic.h>
#include <kiwi/arch/mp.h>
#include <kiwi/arch/irq.h>
#include <kiwi/arch/x86_64.h>
#include <kiwi/timer.h>
#include <kiwi/debug.h>

#define CALIBRATION_PERIOD      (5 * MILLISECOND)
#define CALIBRATION_TIMES       5

static int registered = 0;
static int irq_installed = 0;
static TimerDevice lapic_timer = {
    .name = "local APIC",
    .frequency = NULL,
    .read = NULL,
    .busy_wait = NULL,
    .set_alarm = NULL,
    .get_alarm = NULL,
    .cancel_alarm = NULL,
    .count = 1,
    .enabled = 0,
    .global = 0,
    .direction = -1,
};

void lapic_timer_irq_stub();

u64 lapic_timer_frequency(u32 unused) {
    CPUInfo *cpu_info = arch_get_current_cpu_info();
    return cpu_info->local_apic->timer_frequency;
}

u64 lapic_timer_read(u32 unused) {
    return lapic_read(LAPIC_TIMER_CURRENT_COUNT);
}

void lapic_timer_busy_wait(u32 unused, u64 ts) {
    while(lapic_read(LAPIC_TIMER_CURRENT_COUNT) > ts) {
        arch_spin_backoff();
    }
}

int lapic_timer_set_alarm(u32 unused, u64 ts) {
    CPUInfo *cpu_info = arch_get_current_cpu_info();
    LocalAPIC *lapic = cpu_info->local_apic;
    lapic->timer_alarm = ts;
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, ts);
    return 0;
}

u64 lapic_timer_get_alarm(u32 unused) {
    CPUInfo *cpu_info = arch_get_current_cpu_info();
    LocalAPIC *lapic = cpu_info->local_apic;
    return lapic->timer_alarm;
}

void lapic_timer_cancel_alarm(u32 unused) {
    CPUInfo *cpu_info;
    LocalAPIC *lapic;
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
    cpu_info = arch_get_current_cpu_info();
    lapic = cpu_info->local_apic;
    lapic->timer_alarm = 0;
}

static inline void dummy_bubblesort(u64 *arr, int n) {
    /* this objectively sucks but we know that n is small anyway and it's not
     * used anywhere else. never let my algos prof see this or he might revoke
     * the A he gave me.
     */
    for(int i = 0; i < n - 1; i++) {
        for(int j = 0; j < n - i - 1; j++) {
            if(arr[j] > arr[j + 1]) {
                u64 temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

static inline u64 median(u64 *arr, int n) {
    dummy_bubblesort(arr, n);
    if(n % 2)
        return arr[n/2];
    return (arr[n/2-1] + arr[n/2]) / 2;
}

int lapic_timer_init(void) {
    CPUInfo *cpu_info = arch_get_current_cpu_info();
    LocalAPIC *lapic = cpu_info->local_apic;
    u32 initial_counter, final_counter;
    u64 frequencies[CALIBRATION_TIMES];
    int timer_index;

    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);  // disable timer
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_ONESHOT | LAPIC_LVT_MASK);
    lapic_write(LAPIC_TIMER_DIVIDE_CONFIG, LAPIC_TIMER_DIVIDER_1);

    /* calibrate multiple times and then use the median */
    for(int i = 0; i < CALIBRATION_TIMES; i++) {
        lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0xFFFFFFFF); // maximum initial count

        initial_counter = lapic_read(LAPIC_TIMER_CURRENT_COUNT);
        timer_block_for(CALIBRATION_PERIOD);
        final_counter = lapic_read(LAPIC_TIMER_CURRENT_COUNT);

        lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
        frequencies[i] = ((initial_counter - final_counter)
            * 1000000000ULL) / CALIBRATION_PERIOD;
    }

    lapic->timer_frequency = median(frequencies, CALIBRATION_TIMES);
    debug_info("local APIC [%d] timer @ %llu MHz",
        cpu_info->index, lapic->timer_frequency / 1000000);

    /* enable oneshot mode and configure IRQ */
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_ONESHOT | LAPIC_TIMER_VECTOR);

    /* divider 1 gives us an insane level of precision that's frankly really
     * overkill, but it is mentally the simplest to reason about + we don't need
     * to worry about overflows because we are going to be using this timer for
     * literally all our scheduling needs, so we are kinda guaranteed that the
     * timeouts will be short enough to fit in a 32-bit word even at max freq,
     * and honestly who's gonna say no to nanosecond precision?
     */
    lapic_write(LAPIC_TIMER_DIVIDE_CONFIG, LAPIC_TIMER_DIVIDER_1);

    if(!irq_installed) {
        if(arch_install_isr(LAPIC_TIMER_VECTOR, (uptr) lapic_timer_irq_stub,
            GDT_KERNEL_CODE << 3, 0)) {
            debug_error("failed to install local APIC timer IRQ handler");
            return -1;
        }
        irq_installed = 1;
    }

    if(!registered) {
        lapic_timer.frequency = lapic_timer_frequency;
        lapic_timer.read = lapic_timer_read;
        lapic_timer.busy_wait = lapic_timer_busy_wait;
        lapic_timer.set_alarm = lapic_timer_set_alarm;
        lapic_timer.get_alarm = lapic_timer_get_alarm;
        lapic_timer.cancel_alarm = lapic_timer_cancel_alarm;
        lapic_timer.enabled = 1;

        timer_index = timer_register(&lapic_timer);
        if(timer_index < 0) {
            debug_error("failed to register local APIC timer device");
            return -1;
        }

        if(timer_set_default(timer_index, 0) < 0) {
            debug_error("failed to set local APIC timer as default timer");
            return -1;
        }
        registered = 1;
    }

    return 0;
}

void lapic_timer_irq(IRQStackFrame *frame) {
    arch_ack_irq(NULL);
}
