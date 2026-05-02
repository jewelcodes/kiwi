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
#include <kiwi/worker.h>

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
    /* trigger immediately if ts is 0 */
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, ts ? ts : 1);
    return 0;
}

u64 lapic_timer_get_alarm(u32 unused) {
    return lapic_read(LAPIC_TIMER_CURRENT_COUNT);
}

void lapic_timer_cancel_alarm(u32 unused) {
    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
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

static inline void lapic_timer_set_divider(int divider) {
    u32 divider_config;

    switch(divider) {
    case 1:
        divider_config = LAPIC_TIMER_DIVIDER_1;
        break;
    case 2:
        divider_config = LAPIC_TIMER_DIVIDER_2;
        break;
    case 4:
        divider_config = LAPIC_TIMER_DIVIDER_4;
        break;
    case 8:
        divider_config = LAPIC_TIMER_DIVIDER_8;
        break;
    case 16:
        divider_config = LAPIC_TIMER_DIVIDER_16;
        break;
    case 32:
        divider_config = LAPIC_TIMER_DIVIDER_32;
        break;
    case 64:
        divider_config = LAPIC_TIMER_DIVIDER_64;
        break;
    case 128:
    default:
        divider_config = LAPIC_TIMER_DIVIDER_128;
        break;
    }

    lapic_write(LAPIC_TIMER_DIVIDE_CONFIG, divider_config);
}

static u64 lapic_calibrate_timer(int divider) {
    /* anecdotally, on QEMU (TCG), I seem to get slightly different results the
     * first time I calibrate the timer, and I have observed that prolonging the
     * calibration period doesn't meaningfully reduces the variance, which is
     * why we calibrate several times and then take the median.
     */
    u32 initial_counter, final_counter;
    u64 frequencies[CALIBRATION_TIMES];

    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0); // disable timer
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_ONESHOT | LAPIC_LVT_MASK);
    lapic_timer_set_divider(divider);

    /* calibrate multiple times and then use the median */
    for(int i = 0; i < CALIBRATION_TIMES; i++) {
        lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0xFFFFFFFF); // maximum initial count

        initial_counter = lapic_read(LAPIC_TIMER_CURRENT_COUNT);
        timer_block_for(CALIBRATION_PERIOD);
        final_counter = lapic_read(LAPIC_TIMER_CURRENT_COUNT);

        lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
        frequencies[i] = ((u64) initial_counter - final_counter)
            * SECOND / CALIBRATION_PERIOD;
    }

    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
    return median(frequencies, CALIBRATION_TIMES);
}

int lapic_timer_init(void) {
    CPUInfo *cpu_info = arch_get_current_cpu_info();
    LocalAPIC *lapic = cpu_info->local_apic;
    int timer_index, divider;
    u64 frequency;

    /* the goal here is to not only measure the frequency of the timer, but also
     * to find a good divider to use. We want a divider that gives a frequency
     * that is high enough to provide good resolution for short alarms, but also
     * also low enough to allow us to not worry about overflows, especially
     * because the local APIC timer is (unfortunately) just 32 bits.
     */
    for(divider = 128; divider >= 1; divider /= 2) {
        frequency = lapic_calibrate_timer(divider);
        if(frequency >= 10000 && frequency <= 100000000)
            goto configure_timer;
    }

    /* if we get here, we didn't find a frequency that met our criteria, so pick
     * the lowest frequency so we get to at least avoid worrying about overflows
     * as much as possible. this is not ideal but it's also technically not a
     * cause for error.
     */
    divider = 128;
    frequency = lapic_calibrate_timer(divider);
    debug_warn("local APIC timer frequency is outside the ideal range");

configure_timer:
    lapic->timer_frequency = frequency;
    debug_info("local APIC [%d] timer @ %llu %cHz (divider %d)",
        cpu_info->index,
        lapic->timer_frequency > 1000000
            ? lapic->timer_frequency / 1000000
            : lapic->timer_frequency / 1000,
        lapic->timer_frequency > 1000000 ? 'M' : 'K',
        divider);

    /* enable oneshot mode and configure IRQ */
    if(!irq_installed) {
        debug_info("installing local APIC timer IRQ handler...");
        if(arch_install_isr(LAPIC_TIMER_VECTOR, (uptr) lapic_timer_irq_stub,
            GDT_KERNEL_CODE << 3, 0)) {
            debug_error("failed to install local APIC timer IRQ handler");
            return -1;
        }
        irq_installed = 1;
    }

    lapic_write(LAPIC_TIMER_INITIAL_COUNT, 0);
    lapic_timer_set_divider(divider);
    lapic_write(LAPIC_LVT_TIMER, LAPIC_TIMER_ONESHOT | LAPIC_TIMER_VECTOR);

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
    worker_alarm((MachineContext *) frame);
    arch_ack_irq(NULL);
}
