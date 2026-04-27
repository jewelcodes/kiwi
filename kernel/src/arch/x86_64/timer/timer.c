/*
 * kiwi - general-purpose high-performance operating system
 * 
 * Copyright (c) 2025-26 Omar Elghoul
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

#include <kiwi/debug.h>
#include <kiwi/timer.h>
#include <kiwi/arch/hpet.h>
#include <kiwi/arch/apic.h>

void arch_timer_init(void) {
    /* We need to start with the HPET or PIT first because the PIT has a fixed
     * frequency and the HPET directly reports its frequency. Either can be used
     * to calibrate the local APIC timer, which will be used as the main timing
     * source for the workers and scheduler.
     * 
     * TODO: actually implement the PIT fallback!
     */
    int status;
    status = hpet_init();
    if(status)
        debug_panic("failed to initialize HPET while PIT fallback is unimplemented");
    
    status = lapic_timer_init();
    if(status)
        debug_panic("failed to initialize local APIC timer");
}
