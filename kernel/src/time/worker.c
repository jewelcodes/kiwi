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
#include <kiwi/ipi.h>
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

static inline int cpu_pending_work(int cpu) {
    Worker *worker = cpu_to_worker(cpu);
    if(!worker)
        return 0;
    return worker->ready_work->size + worker->incoming_work->size;
}

static inline int get_busiest_cpu(void) {
    int cpu_count = arch_get_cpu_count();
    int busiest = 0;
    int max_pending = cpu_pending_work(0);
    int pending;

    for(int i = 1; i < cpu_count; i++) {
        pending = cpu_pending_work(i);
        if(pending > max_pending) {
            busiest = i;
            max_pending = pending;
        }
    }
    return busiest;
}

static inline int get_least_busy_cpu(void) {
    Worker *worker;
    int cpu_count = arch_get_cpu_count();
    int least_busy = 0;
    int min_pending = cpu_pending_work(0);
    int pending;

    for(int i = 1; i < cpu_count; i++) {
        /* early return in the case we find a CPU that is actually not doing
         * anything rn, regardless of whether it has work coming in or not.
         */
        worker = cpu_to_worker(i);
        if(!worker->current_work)
            return i;
        pending = cpu_pending_work(i);
        if(pending < min_pending) {
            least_busy = i;
            min_pending = pending;
        }
    }
    return least_busy;
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

static int needs_load_balancing(Worker *worker) {
    WorkItem *next;
    u64 ts_est_completion, ts_next_ready;

    if(arch_get_cpu_count() <= 1)
        return 0;

    /* fun fact, this conditional below was actually inspired by a paper about
     * network congestion avoidance, where it was proposed that a good heuristic
     * for determining whether to signal the sender to slow down was if the
     * incoming queue had an average length of >= 1 over an interval of time.
     * this is an extremely simple heuristic and can seem super aggressive at a
     * glance, but I really like it because the existence of even just one item
     * in the ready queue means we already have an item that is due now or
     * past-due, and yet we do not have the resources to execute it immediately;
     * the resource being CPU time in this case. I wrote a 10-page paper on this
     * back in school and I still believe it is one of the most elegant and
     * brilliant heuristics out there for proactively avoiding congestion, or in
     * our case, overload :)
     * 
     * the original paper is titled "A Binary Feedback Scheme for Congestion
     * Avoidance in Computer Networks" (https://doi.org/10.1145/78952.78955) if
     * anyone reading this is curious, I would very highly recommend it.
     * shoutout to my college professor John Day for recommending I read it back
     * in the day
     */
    if(worker->ready_work->size)
        return 1;

    /* in the same proactive spirit of the heuristic above, let's also make a
     * load balancing event happen if we estimate that the current work item is
     * still gonna be running by the time the next incoming work is due.
     */
    if(worker->current_work && worker->current_work->max_runtime) {
        ts_est_completion = uptime_ms() + worker->current_work->max_runtime;
        next = pq_peek(worker->incoming_work, &ts_next_ready);
        return next && (ts_est_completion >= ts_next_ready);
    }

    return 0;
}

static void do_load_balancing(Worker *worker) {
    int least_busy;

    if(arch_get_cpu_count() <= 1)
        return;
    least_busy = get_least_busy_cpu();
    if(least_busy == arch_get_current_cpu())
        return;
    ipi_send(least_busy, IPI_MESSAGE_WAKEUP, NULL);
}

TIMER_CALLBACK(worker_alarm) {
    Worker *worker = get_current_worker();
    WorkItem *work;
    u64 runtime;

    if(!worker)
        return;
    work = worker->current_work;
    if(!work)
        return;

    runtime = uptime_ms() - work->ts_started;
    if(runtime >= work->max_runtime) {
        work->status = WORK_INTERRUPTED;
        arch_set_context(ctx, &worker->restore_context);
    }
}

static void steal_work(void *unused) {
    Worker *me, *victim;
    WorkItem *work;
    int busiest;

    busiest = get_busiest_cpu();
    me = get_current_worker();
    victim = cpu_to_worker(busiest);
    if(!me || !victim || victim == me)
        return;

    /* to reduce the overhead from locking, we will try to steal half of the
     * victim's work per IPI so that the lock can be held for a short time
     */
    arch_spinlock_acquire(&victim->ready_lock);
    arch_spinlock_acquire(&me->ready_lock);
    for(usize i = 0; i < (victim->ready_work->size+1) / 2; i++) {
        work = pq_pop(victim->ready_work, NULL);
        if(!work)
            break;
        work->cpu = arch_get_current_cpu();
        pq_push(me->ready_work, work->ts_ready, work);
    }
    arch_spinlock_release(&me->ready_lock);
    arch_spinlock_release(&victim->ready_lock);

    arch_spinlock_acquire(&victim->incoming_lock);
    arch_spinlock_acquire(&me->incoming_lock);
    for(usize i = 0; i < (victim->incoming_work->size+1) / 2; i++) {
        work = pq_pop(victim->incoming_work, NULL);
        if(!work)
            break;
        work->cpu = arch_get_current_cpu();
        pq_push(me->incoming_work, work->ts_ready, work);
    }
    arch_spinlock_release(&me->incoming_lock);
    arch_spinlock_release(&victim->incoming_lock);

    check_and_set_alarm(me, NULL);
}

IPI_CALLBACK(worker_ipi_alarm) {
    /* we only care about wakeups here */
    if(!ipi_message_is_wakeup(msg))
        return;
    work_create("steal_work", steal_work, NULL);
}

void worker_init(void) {
    CPUInfo *cpu_info;
    Worker *cpu_worker;
    int status;

    debug_info("initializing deferred work subsystem...");

    status = timer_subscribe(worker_alarm);
    if(status < 0)
        debug_panic("failed to subscribe worker alarm callback to timer");
    status = ipi_subscribe(worker_ipi_alarm);
    if(status < 0)
        debug_panic("failed to subscribe worker IPI callback");

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
        status = arch_create_kernel_context(&cpu_worker->restore_context, worker_loop, NULL);
        if(status < 0)
            debug_panic("failed to create kernel context for worker loop on CPU %d", i);

        cpu_info->worker = cpu_worker;
    }
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
    arch_spinlock_acquire(&worker->incoming_lock);
    pq_push(worker->incoming_work, ts_ready, work);
    arch_spinlock_release(&worker->incoming_lock);
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

void worker_loop(void *unused) {
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
        arch_disable_irqs();

        while(me->incoming_work->size > 0) {
            arch_spinlock_acquire(&me->incoming_lock);
            work = (WorkItem *) pq_peek(me->incoming_work, &ts_ready);
            if(ts_ready > uptime_ms() + LATE_THRESHOLD) {
                arch_spinlock_release(&me->incoming_lock);
                break;      /* we're early, work isn't ready yet */
            }

            work = (WorkItem *) pq_pop(me->incoming_work, &ts_ready);
            arch_spinlock_acquire(&me->ready_lock);
            pq_push(me->ready_work, ts_ready, work);
            work->status = WORK_READY;
            arch_spinlock_release(&me->ready_lock);
            arch_spinlock_release(&me->incoming_lock);
        }

        /* exhaust the ready queue */
        while(me->ready_work->size > 0) {
            arch_spinlock_acquire(&me->ready_lock);
            work = (WorkItem *) pq_pop(me->ready_work, &ts_ready);
            arch_spinlock_release(&me->ready_lock);

            arch_spinlock_acquire(&work->lock);
            if(work->status != WORK_CANCELLED) {
                me->current_work = work;
                work->status = WORK_RUNNING;

                if(needs_load_balancing(me))
                    do_load_balancing(me);
                if(work->max_runtime)
                    timer_set_alarm_after(work->max_runtime);

                arch_enable_irqs();
                work->ts_started = uptime_ms();
                work->func(work->arg);
                work->ts_finished = uptime_ms();
                work->status = WORK_FINISHED;
                arch_disable_irqs();
            }

            arch_spinlock_release(&work->lock);
            me->current_work = NULL;
        }

        /* here we've reached the idle state, so it is time to check for work
         * and update the alarm if necessary.
         */
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
