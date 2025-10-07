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

#define HPET_GENERAL_CAP                        0x000
#define HPET_GENERAL_CONFIG                     0x010
#define HPET_GENERAL_IRQ_STATUS                 0x020
#define HPET_MAIN_COUNTER                       0x0F0
#define HPET_TIMER_CONFIG_CAP                   0x100
#define HPET_TIMER_COMPARATOR                   0x108
#define HPET_TIMER_FSB_INT_ROUTE                0x110

#define HPET_TIMER_STRIDE                       0x20

#define HPET_GENERAL_CAP_TIMER_COUNT(x)         ((x >> 8) & 0x1F)
#define HPET_GENERAL_CAP_64BIT_COUNTER          0x2000
#define HPET_GENERAL_CAP_COUNTER_PERIOD(x)      ((x >> 32) & 0xFFFFFFFF)

#define HPET_GENERAL_CONFIG_ENABLE              0x1

#define HPET_TIMER_CONFIG_CAP_TRIGGER_LEVEL     0x02
#define HPET_TIMER_CONFIG_CAP_ENABLE            0x04
#define HPET_TIMER_CONFIG_CAP_PERIODIC          0x08
#define HPET_TIMER_CONFIG_CAP_PERIODIC_CAP      0x10
#define HPET_TIMER_CONFIG_CAP_SIZE_64BIT        0x20
#define HPET_TIMER_CONFIG_CAP_PERIODIC_VAL_SET  0x40

int hpet_init(void);
u64 hpet_frequency(void);
void hpet_block(u64 ns);

typedef struct HPETTable {
    ACPIHeader header;
    u32 event_timer_block_id;
    ACPIAddress base_address;
    u32 reserved[2];
    u8 hpet_number;
    u16 minimum_tick;
    u8 page_protection;
} __attribute__((packed)) HPETTable;
