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

#include <kiwi/arch/atomic.h>
#include <kiwi/arch/context.h>
#include <kiwi/structs/priorityqueue.h>

#define WORK_PENDING        0
#define WORK_READY          1
#define WORK_RUNNING        2
#define WORK_CANCELLED      3
#define WORK_INTERRUPTED    4
#define WORK_FINISHED       5

typedef struct WorkItem WorkItem;

typedef struct Worker {
    /* incoming_work is only ever accessed by the local CPU, so it doesn't
     * need to be locked.
     *
     * ready_work can be accessed by other CPUs with work stealing (which
     * is not implemented yet but will be eventually), so it need a lock.
     * to reduce this contention, the CPUs stealing work will steal half
     * of the items in the queue at a time, so the lock will only be held
     * for a very short time.
     */
    PriorityQueue *incoming_work;
    PriorityQueue *ready_work;
    lock_t ready_lock;
    WorkItem *current_work;
    MachineContext new_context;
    int needs_context_switch;
} Worker;

struct WorkItem {
    char name[64];
    void (*func)(void *);
    void *arg;
    u64 ts_created;
    u64 ts_ready;       /* work items are sorted by this value */
    u64 max_runtime;    /* maximum allowed runtime before interrupting */
    u64 ts_started;
    u64 ts_finished;
    int status;
    int cpu;            /* current CPU work is assigned to. if the work has not
                         * started yet, this field may not accurately reflect
                         * which CPU the work will run on.
                         * 
                         * TODO: are there cases where we might want to change
                         * this and make a flag that prevents stealing certain
                         * work items?
                         */
    lock_t lock;        /* used to synchronize exec and cancel */
};

void worker_init(void);
void worker_alarm(MachineContext *ctx);
Worker *get_current_worker(void);
Worker *cpu_to_worker(int index);
WorkItem *work_create_timeboxed(const char *name, void (*func)(void *),
                                void *arg, u64 ts_ready, u64 max_runtime);
WorkItem *work_create(const char *name, void (*func)(void *), void *arg);
int work_cancel(WorkItem *work);
void work_destroy(WorkItem *work);
int work_status(WorkItem *work);
void worker_loop(void);
