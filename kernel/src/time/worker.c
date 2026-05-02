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
#include <kiwi/arch/irq.h>
#include <kiwi/debug.h>
#include <kiwi/worker.h>
#include <kiwi/timer.h>
#include <stdlib.h>
#include <string.h>

#define ALARM_UNCHANGED                 0
#define ALARM_UNCHANGED_WORK_IS_LATE    1
#define ALARM_UPDATED                   2

/* because the timer hardware does not necessarily provide nanosecond
 * precision and because there is some latency and overhead to everything,
 * we want to avoid setting alarms that are virtually going to trigger
 * immediately anyway. so just return early in this case and assume the
 * work is due now or even late.
 *
 * The acceptable threshold we will use for "virtually immediate" is 1 ms.
 * in all honesty this was a truly arbitrary choice and I may change it in
 * the future or even make it configurable, but for now I feel like this is
 * a reasonable educated guess and a good starting point for calibration.
 */

#define LATE_THRESHOLD      (-((s64) MILLISECOND))

void worker_init(void) {
    CPUInfo *cpu_info;
    Worker *cpu_worker;
    debug_info("initializing deferred work subsystem...");

    for(int i = 0; i < arch_get_cpu_count(); i++) {
        cpu_info = arch_get_cpu_info(i);
        if(!cpu_info)
            debug_panic("failed to get CPU info for CPU %d", i);
        
        cpu_worker = (Worker *) calloc(1, sizeof(Worker));
        if(!cpu_worker)
            debug_panic("failed to create worker for CPU %d", i);

        cpu_worker->ready_work = pq_create();
        cpu_worker->incoming_work = pq_create();
        cpu_worker->ready_lock = LOCK_INITIAL;
        if(!cpu_worker->ready_work || !cpu_worker->incoming_work)
            debug_panic("failed to create work queues for CPU %d", i);
        cpu_info->worker = cpu_worker;
    }
}

void worker_alarm(MachineContext *ctx) {
    /* TODO: is this a good place to implement a context switch? */
}

Worker *get_current_worker(void) {
    CPUInfo *cpu_info = arch_get_current_cpu_info();
    if(!cpu_info)
        return NULL;
    return cpu_info->worker;
}

Worker *cpu_to_worker(int index) {
    CPUInfo *cpu_info = arch_get_cpu_info(index);
    if(!cpu_info)
        return NULL;
    return cpu_info->worker;
}

static int check_and_set_alarm(Worker *worker, u64 *delta_alarm_out) {
    /* If the ready time of the next incoming work is earlier than the currently
     * set alarm, then we need to update the alarm to reflect the new work.
     * If there's no incoming work, or if the next incoming work is already late,
     * then we will not change the alarm and let the worker do its thing.
     */
    WorkItem *work;
    u64 ts_ready;
    s64 delta_ready, delta_alarm, delta;

    if(worker->incoming_work->size == 0)
        return ALARM_UNCHANGED;
    
    work = pq_peek(worker->incoming_work, &ts_ready);
    if(!work)
        return ALARM_UNCHANGED;

    delta_ready = (s64) (ts_ready - (s64) uptime_ms());
    if(delta_ready <= LATE_THRESHOLD)
        return ALARM_UNCHANGED_WORK_IS_LATE;

    delta_alarm = timer_get_alarm();
    delta = delta_ready - delta_alarm;
    if(delta <= LATE_THRESHOLD || !delta_alarm) {
        timer_set_alarm_at(ts_ready);
        if(delta_alarm_out)
            *delta_alarm_out = delta_alarm;
        return ALARM_UPDATED;
    }

    return ALARM_UNCHANGED;
}

WorkItem *work_create_timeboxed(const char *name, void (*func)(void *),
                                void *arg, u64 ts_ready, u64 max_runtime) {
    Worker *worker = get_current_worker();
    WorkItem *work;
    if(!worker)
        return NULL;

    work = calloc(1, sizeof(WorkItem));
    if(!work)
        return NULL;

    strncpy(work->name, name, sizeof(work->name) - 1);
    work->name[sizeof(work->name) - 1] = '\0';
    work->func = func;
    work->arg = arg;
    work->ts_created = uptime_ms();
    work->ts_ready = ts_ready;
    work->max_runtime = max_runtime;
    work->cpu = arch_get_current_cpu();
    work->lock = LOCK_INITIAL;

    if(ts_ready <= uptime_ms() + LATE_THRESHOLD) {
        /* if the work is already late, just put it in the ready queue directly
         * instead of setting an alarm for it, otherwise it may end up waiting
         * until the next alarm to be processed even though it's already due.
         */
        work->status = WORK_READY;
        arch_spinlock_acquire(&worker->ready_lock);
        pq_push(worker->ready_work, ts_ready, work);
        arch_spinlock_release(&worker->ready_lock);
        return work;
    }

    work->status = WORK_PENDING;
    pq_push(worker->incoming_work, ts_ready, work);
    check_and_set_alarm(worker, NULL);
    return work;
}

WorkItem *work_create(const char *name, void (*func)(void *), void *arg) {
    return work_create_timeboxed(name, func, arg, 0, 0);
}

int work_cancel(WorkItem *work) {
    int status;
    if(!work)
        return -1;

    if(!arch_spinlock_try_acquire(&work->lock))
        return -1;      /* work is currently executing, cannot cancel */
    
    if(work->status == WORK_CANCELLED || work->status == WORK_FINISHED) {
        status = -1;    /* nothing to do */
        goto release_and_out;
    }

    work->status = WORK_CANCELLED;
    status = 0;
release_and_out:
    arch_spinlock_release(&work->lock);
    return status;
}

void work_destroy(WorkItem *work) {
    if(work)
        free(work);
}

void worker_loop(void) {
    Worker *me = NULL;
    WorkItem *work;
    u64 ts_ready;
    int status;

    while(!me) {
        /* this is necessary for early boot, because APs end up here before we
         * have initialized the worker subsystem from the boot CPU
         */
        me = get_current_worker();
        if(!me)
            arch_halt_until_irq();
    }

    for(;;) {
        arch_enable_irqs();

        /* move work from the incoming queue into the ready queue if it's time.
         * we are using locks ONLY with the ready queue because these will
         * someday be stolen by other CPUs when I implement work stealing, but
         * the incoming queue is only ever pushed to by the local CPU so it
         * doesn't need to be locked.
         */
        while(me->incoming_work->size > 0) {
            work = (WorkItem *) pq_peek(me->incoming_work, &ts_ready);
            if(ts_ready > uptime_ms() + LATE_THRESHOLD)
                break;      /* we're early, work isn't ready yet */

            work = (WorkItem *) pq_pop(me->incoming_work, &ts_ready);
            arch_spinlock_acquire(&me->ready_lock);
            pq_push(me->ready_work, ts_ready, work);
            work->status = WORK_READY;
            arch_spinlock_release(&me->ready_lock);
        }

        /* exhaust the ready queue */
        while(me->ready_work->size > 0) {
            arch_spinlock_acquire(&me->ready_lock);
            work = (WorkItem *) pq_pop(me->ready_work, &ts_ready);
            arch_spinlock_release(&me->ready_lock);
            if(!work)
                break;      /* work possibly got stolen by another CPU */

            arch_spinlock_acquire(&work->lock);
            if(work->status != WORK_CANCELLED) {
                me->current_work = work;
                work->status = WORK_RUNNING;
                work->ts_started = uptime_ms();
                work->func(work->arg);
                work->ts_finished = uptime_ms();
            }

            arch_spinlock_release(&work->lock);
            me->current_work = NULL;
        }

        /* here we've reached the idle state, so it is time to check for work
         * and update the alarm if necessary. we disable irqs here to avoid the
         * off-chance where the alarm we set gets immediately handled before we
         * go to sleep, making us miss it.
         */
        arch_disable_irqs();
        status = check_and_set_alarm(me, NULL);
        if(status == ALARM_UNCHANGED_WORK_IS_LATE)
            continue;

        /* at this point, there truly is no more work in any of the queues, so
         * we will go to sleep until another CPU wakes us up with an IPI. this
         * function also automatically re-enables interrupts before halting, so
         * we don't need to worry about missing irqs here
         */
        arch_halt_until_irq();
    }
}