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
#include <kiwi/arch/context.h>

#define IPI_MESSAGE_WAKEUP                  0x00000001
#define IPI_MESSAGE_TLB_SHOOTDOWN           0x00000002
#define IPI_MESSAGE_BROADCAST               0x80000000

#define IPI_MESSAGE_MASK(x)                 ((x) & 0x7FFFFFFF)

typedef struct IPIMessage {
    void *opaque;
    u32 message_type;
    int src;
    int dst;
} IPIMessage;

typedef void (*IPICallback)(IPIMessage *msg, MachineContext *ctx);

#define IPI_CALLBACK(f)         void f(IPIMessage *msg, MachineContext *ctx)

static inline int ipi_message_is_wakeup(IPIMessage *msg) {
    return (!msg || (IPI_MESSAGE_MASK(msg->message_type) == IPI_MESSAGE_WAKEUP));
}

extern Array *ipi_subscribers;

void ipi_init(void);
int ipi_subscribe(IPICallback callback);
int ipi_unsubscribe(IPICallback callback);
int ipi_send(int dst, u32 message_type, void *opaque);
int ipi_broadcast(u32 message_type, void *opaque);
void ipi_handler(MachineContext *ctx);
