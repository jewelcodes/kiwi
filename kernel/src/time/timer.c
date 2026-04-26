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
    if(!device)
        return -1;
    if(!device->enabled || !device->frequency || !device->read || !device->busy_wait)
        return -1;

    status = array_push(timer_devices, (u64) device);
    if(status)
        return -1;

    supports_alarms = device->set_alarm && device->get_alarm && device->cancel_alarm;
    debug_info("registered device '%s' with %d timers", device->name,
        device->count);
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

u64 uptime_ns(void) {
    TimerDevice *dev = default_global_dev ? default_global_dev : default_dev;
    int timer_index = default_global_timer >= 0 ? default_global_timer : default_timer;
    u64 frequency, ticks;

    if(!dev || timer_index < 0)
        return 0;
    ticks = dev->read(timer_index);
    frequency = dev->frequency(timer_index);
    if(!ticks || !frequency)
        return 0;
    return (ticks * 1000000000ULL) / frequency;
}
