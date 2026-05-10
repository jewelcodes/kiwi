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

#include <kiwi/arch/mp.h>
#include <kiwi/debug.h>
#include <kiwi/ipi.h>
#include <stdlib.h>

Array *ipi_subscribers = NULL;
static lock_t lock = LOCK_INITIAL;

void ipi_init(void) {
    ipi_subscribers = array_create();
    if(!ipi_subscribers)
        debug_panic("failed to create IPI subscribers array");
}

int ipi_subscribe(IPICallback callback) {
    int status;

    if(!callback)
        return -1;

    arch_spinlock_acquire(&lock);
    status = array_push(ipi_subscribers, (u64) callback);
    arch_spinlock_release(&lock);
    return status;
}

int ipi_unsubscribe(IPICallback callback) {
    int status = -1;

    if(!callback)
        return status;

    arch_spinlock_acquire(&lock);
    for(u64 i = 0; i < ipi_subscribers->count; i++) {
        if((IPICallback) ipi_subscribers->items[i] == callback) {
            status = array_delete_index(ipi_subscribers, i);
            break;
        }
    }
    arch_spinlock_release(&lock);
    return status;
}

static IPIMessage *create_ipi_message(u32 message_type, void *opaque) {
    IPIMessage *msg = NULL;

    /* wakeups will happen frequently as part of load-balancing, and so to
     * reduce latency, we want to avoid allocating a message for this, as the
     * message wouldn't really contain any useful info in this case.
     */
    if(IPI_MESSAGE_MASK(message_type) == IPI_MESSAGE_WAKEUP)
        goto out;

    msg->opaque = opaque;
    msg->message_type = message_type;
    msg->src = arch_get_current_cpu();
    msg->dst = -1;

out:
    return msg;
}

int ipi_send(int dst, u32 message_type, void *opaque) {
    IPIMessage *msg = create_ipi_message(IPI_MESSAGE_MASK(message_type), opaque);
    int status;

    if(message_type != IPI_MESSAGE_WAKEUP && !msg)
        return -1;

    status = arch_send_ipi(dst, msg);
    if(status && msg)
        free(msg);
    return status;
}

int ipi_broadcast(u32 message_type, void *opaque) {
    IPIMessage *msg = create_ipi_message(message_type, opaque);
    int status;

    if(message_type != IPI_MESSAGE_WAKEUP && !msg)
        return -1;

    status = arch_broadcast_ipi(msg);
    if(status && msg)
        free(msg);
    return status;
}

static void do_ipi_callbacks(IPIMessage *msg, MachineContext *ctx) {
    IPICallback callback;
    for(u64 i = 0; i < ipi_subscribers->count; i++) {
        callback = (IPICallback) ipi_subscribers->items[i];
        if(callback)
            callback(msg, ctx);
    }
}

void ipi_handler(MachineContext *ctx) {
    IPIMessage *msg = NULL;
    CPUInfo *me = arch_get_current_cpu_info();

    if(!me)
        return;

    arch_spinlock_acquire(&me->ipi_queue_lock);

    /* if the queue is empty, this was a wakeup IPI request */
    if(me->ipi_queue && me->ipi_queue->count > 0) {
        msg = NULL; /* to prevent duplications in the off-chance that somehow we can't pop??? */
        array_pop_front(me->ipi_queue, (u64 *) &msg);
        do_ipi_callbacks(msg, ctx);
        if(msg)
            free(msg);
    } else {
        do_ipi_callbacks(NULL, ctx);
    }

    arch_spinlock_release(&me->ipi_queue_lock);
}