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
#include <kiwi/arch/timer.h>
#include <kiwi/timer.h>
#include <kiwi/debug.h>

Array *timer_devices = NULL;
static lock_t lock = LOCK_INITIAL;
static TimerDevice *default_dev = NULL;
static TimerDevice *default_global_dev = NULL;
static int default_timer = -1;
static int default_global_timer = -1;

void timer_init(void) {
    debug_info("initializing timer subsystem...");
    timer_devices = array_create();
    if(!timer_devices)
        debug_panic("failed to create timer devices array");

    arch_timer_init();

    if(!default_dev || default_timer < 0)
        debug_panic("no default timer was set by a driver, nothing to do");
    if(!default_global_dev || default_global_timer < 0)
        debug_panic("no default global timer was set by a driver, nothing to do");
}

int timer_register(TimerDevice *device) {
    int supports_alarms, status;
    if(!device || !device->name || !device->count)
        return -1;
    if(!device->enabled || !device->frequency || !device->read || !device->busy_wait)
        return -1;
    if(device->direction != -1 && device->direction != 1)
        return -1;

    status = array_push(timer_devices, (u64) device);
    if(status)
        return -1;

    supports_alarms = device->set_alarm && device->get_alarm && device->cancel_alarm;
    debug_info("registered device '%s' with %d timer%s", device->name,
        device->count, device->count != 1 ? "s" : "");
    if(!supports_alarms) {
        debug_warn("driver for '%s' doesn't support alarms; cannot use as default",
            device->name);
    }
    return timer_devices->count - 1;
}

int timer_set_default(int device_index, int timer_index) {
    TimerDevice *device;
    if(device_index < 0 || device_index >= timer_devices->count)
        return -1;

    device = (TimerDevice *) timer_devices->items[device_index];
    if(timer_index < 0 || timer_index >= device->count)
        return -1;
    if(!device->set_alarm || !device->get_alarm || !device->cancel_alarm) {
        debug_warn("%s: default timer must support alarms", device->name);
        return -1;
    }

    arch_spinlock_acquire(&lock);
    default_dev = device;
    default_timer = timer_index;
    arch_spinlock_release(&lock);

    if(device->count > 1)
        debug_info("set default timer to '%s' timer %d", device->name, timer_index);
    else
        debug_info("set default timer to '%s'", device->name);
    return 0;
}

int timer_set_default_global(int device_index, int timer_index) {
    TimerDevice *device;
    if(device_index < 0 || device_index >= timer_devices->count)
        return -1;

    device = (TimerDevice *) timer_devices->items[device_index];
    if(timer_index < 0 || timer_index >= device->count)
        return -1;
    if(!device->global) {
        debug_warn("%s: default global timer must be marked global", device->name);
        return -1;
    }

    arch_spinlock_acquire(&lock);
    default_global_dev = device;
    default_global_timer = timer_index;
    arch_spinlock_release(&lock);
    return 0;
}

u64 uptime(void) {
    if(default_global_dev && default_global_timer >= 0)
        return default_global_dev->read(default_global_timer);
    else if(default_dev && default_timer >= 0)
        return default_dev->read(default_timer);
    return 0;
}

u64 uptime_ms(void) {
    TimerDevice *dev = default_global_dev
        ? default_global_dev : default_dev;
    int timer_index = default_global_timer >= 0
         ? default_global_timer : default_timer;
    u64 frequency, ticks;

    if(!dev || timer_index < 0)
        return 0;
    ticks = dev->read(timer_index);
    frequency = dev->frequency(timer_index);
    if(!ticks || !frequency)
        return 0;
    return (ticks * SECOND) / frequency;
}

u64 uptime_ns_fraction(u64 *seconds_out) {
    TimerDevice *dev = default_global_dev
        ? default_global_dev : default_dev;
    int timer_index = default_global_timer >= 0
        ? default_global_timer : default_timer;
    u64 ticks, remainder_ticks, frequency, ns_fraction;

    if(!dev || timer_index < 0) {
        if(seconds_out)
            *seconds_out = 0;
        return 0;
    }

    ticks = dev->read(timer_index);
    frequency = dev->frequency(timer_index);
    remainder_ticks = ticks % frequency;
    ns_fraction = (remainder_ticks * 1000000000ULL) / frequency;
    if(seconds_out)
        *seconds_out = ticks / frequency;
    return ns_fraction;
}

u64 uptime_us_fraction(u64 *seconds_out) {
    return uptime_ns_fraction(seconds_out) / 1000;
}

/* these two functions are wrappers around busy_wait() -- they are not aware of
 * the scheduler or the workers and will literally poll the timers. they should
 * not be used except during very early boot.
 */

void timer_block_for(u64 ms) {
    TimerDevice *dev = default_global_dev ? default_global_dev : default_dev;
    int timer_index = default_global_timer >= 0
        ? default_global_timer : default_timer;
    u64 frequency, ticks;

    if(!dev || timer_index < 0)
        return;
    frequency = dev->frequency(timer_index);
    if(!frequency)
        return;
    ticks = ms * (frequency / SECOND);
    if(dev->direction == -1)
        dev->busy_wait(timer_index, dev->read(timer_index) - ticks);
    else
        dev->busy_wait(timer_index, dev->read(timer_index) + ticks);
}

void timer_block_until(u64 ms) {
    u64 now = uptime_ms();
    if(ms <= now)
        return;
    timer_block_for(ms - now);
}

void timer_set_alarm_after(u64 ms) {
    TimerDevice *dev = default_dev;
    int timer_index = default_timer;
    u64 frequency, delta;

    if(!dev || timer_index < 0)
        return;

    frequency = dev->frequency(timer_index);
    delta = (ms * frequency) / SECOND;
    dev->set_alarm(timer_index, dev->read(timer_index) + delta);
}

void timer_set_alarm_at(u64 ms) {
    u64 delta = ms - uptime_ms();
    if((s64) delta < 0)
        delta = 0;
    timer_set_alarm_after(delta);
}

/* timer_get_alarm(): returns the time REMAINING until the next alarm, zero
 * if no alarm is currently set
 */
u64 timer_get_alarm(void) {
    TimerDevice *dev = default_dev;
    int timer_index = default_timer;
    u64 frequency, alarm;

    if(!dev || timer_index < 0)
        return 0;
    
    frequency = dev->frequency(timer_index);
    if(!frequency)
        return 0;
    alarm = dev->get_alarm(timer_index);
    return (alarm * SECOND) / frequency;
}

void timer_cancel_alarm(void) {
    TimerDevice *dev = default_dev;
    int timer_index = default_timer;

    if(!dev || timer_index < 0)
        return;
    dev->cancel_alarm(timer_index);
}
