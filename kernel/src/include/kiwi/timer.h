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

#pragma once

#include <kiwi/structs/array.h>

#define SECOND          1000000000ULL
#define MILLISECOND     1000000ULL
#define MICROSECOND     1000ULL

typedef struct TimerDevice {
    /* generic backend-independent timer API */
    const char *name;

    /* Each of these functions takes timer index as an argument where the index
     * is <= count. This allows a single device to expose multiple timers which
     * is useful in some cases (e.g. HPET). All timestamps are represented in
     * units of the given timer, so the caller is responsible for converting to
     * or from human-friendly units if necessary.
     * 
     * Required functions by any timer driver:
     * - frequency() returns the frequency of the specified timer in Hz.
     * - read() returns the current timestamp of the specified timer.
     * - busy_wait() literally polls the timer until the specified timestamp is
     *   reached. This should obviously not be used in practice and is only
     *   really useful during early boot before the worker and scheduler are up.
     * 
     * Optional functions for timers that support alarms:
     * - set_alarm() programs the timer to trigger an interrupt when the current
     *   timestamp becomes >= the specified timestamp. If the current timestamp
     *   is already past the specified timestamp, then the interrupt will be
     *   triggered immediately and only once.
     * - get_alarm() returns the timestamp of the currently programmed alarm, or
     *   0 if no alarm is set.
     * - cancel_alarm() cancels the upcoming alarm, if any.
     * 
     * Only one alarm per timer can be programmed at a time. This means that for
     * the core worker thread, we will always end any work by reprogramming the
     * next alarm based on the next event that needs to happen. For this reason,
     * a timer driver must support alarms if it wants to be used as the default
     * timer, as the whole goal here is to create a tickless kernel. We are not
     * exposing any periodic timer APIs as they are currently not needed, but
     * they may be added in the future if we find a use case for them.
     * 
     * Global timers are timers that are guaranteed to be consistent across all
     * CPUs in the system (i.e., timers that live on separate circuits than a
     * CPU core and would return the same value regardless of which CPU reads
     * it.) These timers will be used to implement uptime() and general time
     * accounting for the kernel. Non-global timers may return inconsistent
     * reads depending on which CPU reads them and thus are only useful for
     * local scheduling.
     * 
     * Default timers may or may not be global, but they must support alarms as
     * stated earlier. The kernel will prefer to use a non-global default timer
     * if possible as the timer I/O overhead will likely be lower, but global
     * timers are still useful as a fallback.
     */

    u64 (*frequency)(u32 index);
    u64 (*read)(u32 index);
    void (*busy_wait)(u32 index, u64 ts);
    int (*set_alarm)(u32 index, u64 ts);
    u64 (*get_alarm)(u32 index);
    void (*cancel_alarm)(u32 index);

    u32 count;
    int enabled;
    int global;
} TimerDevice;

extern Array *timer_devices;

void timer_init(void);
int timer_register(TimerDevice *device);
int timer_set_default(int device_index, int timer_index);
int timer_set_default_global(int device_index, int timer_index);
u64 uptime(void);
u64 uptime_ns(void);
