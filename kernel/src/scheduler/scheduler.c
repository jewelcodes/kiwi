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

#include <kiwi/arch/mp.h>
#include <kiwi/arch/atomic.h>
#include <kiwi/arch/context.h>
#include <kiwi/structs/hashmap.h>
#include <kiwi/debug.h>
#include <kiwi/scheduler.h>
#include <stdlib.h>

static int scheduler_enabled = 0;
static u64 *bitmap;
static Hashmap *process_map;
static Hashmap *thread_map;

static lock_t process_map_lock = LOCK_INITIAL;
static lock_t thread_map_lock = LOCK_INITIAL;
static pid_t kernel_pid = -1;

void scheduler_start(void) {
    scheduler_enabled = 1;
}

void scheduler_stop(void) {
    scheduler_enabled = 0;
}

static pid_t allocate_pid(void) {
    for(pid_t pid = 0; pid < MAX_PROCESSES; pid++) {
        u64 volatile *word = (u64 volatile *) &bitmap[pid / 64];
        u64 old = *word;
        u64 mask = (u64) 1ULL << (pid % 64);
        if(!(old & mask)) {
            u64 new = old | mask;
            if(arch_cas64((u64 *) word, old, new)) {
                return pid;
            }

            pid--;
            if(pid < 0) {
                pid = 0;
            }
        }
    }

    return -1;
}

void scheduler_init(void) {
    bitmap = calloc((MAX_PROCESSES + 7) / 8, 1);
    if(!bitmap) {
        debug_panic("failed to allocate memory for process bitmap");
    }

    process_map = hashmap_create();
    thread_map = hashmap_create();
    if(!process_map || !thread_map) {
        debug_panic("failed to create scheduler hashmaps");
    }

    for(int i = 0; i < arch_get_cpu_count(); i++) {
        CPUInfo *cpu_info = arch_get_cpu_info(i);
        if(!cpu_info) {
            debug_panic("failed to get CPU info for CPU %d", i);
        }

        for(int j = 0; j < PRIORITY_COUNT; j++) {
            cpu_info->scheduler_state.ready_queues[j] = cldeque_create();
            if(!cpu_info->scheduler_state.ready_queues[j]) {
                debug_panic("failed to create ready queue for CPU %d", i);
            }
        }

        cpu_info->scheduler_state.current_process = NULL;
        cpu_info->scheduler_state.current_thread = NULL;
        cpu_info->scheduler_state.idle_thread = NULL;
    }

    debug_info("scheduler ready");

    kernel_pid = process_create();
    if(kernel_pid < 0) {
        debug_panic("failed to create kernel process");
    }

    debug_info("created kernel process with PID %d", kernel_pid);
    scheduler_start();
}

Process *get_current_process(void) {
    CPUInfo *cpu_state = arch_get_current_cpu_info();
    return cpu_state ? cpu_state->scheduler_state.current_process : NULL;
}

Thread *get_current_thread(void) {
    CPUInfo *cpu_state = arch_get_current_cpu_info();
    return cpu_state ? cpu_state->scheduler_state.current_thread : NULL;
}

pid_t getpid(void) {
    Process *proc = get_current_process();
    return proc ? proc->pid : kernel_pid;
}

pid_t gettid(void) {
    Thread *thread = get_current_thread();
    return thread ? thread->tid : kernel_pid;
}

pid_t process_create(void) {
    arch_spinlock_acquire(&process_map_lock);

    pid_t pid = allocate_pid();
    if(pid < 0) {
        goto release_and_fail;
    }

    Process *current_process = get_current_process();
    Process *new_process = calloc(1, sizeof(Process));
    if(!new_process) {
        goto release_and_fail;
    }

    new_process->pid = pid;
    new_process->uid = current_process ? current_process->uid : 0;
    new_process->gid = current_process ? current_process->gid : 0;
    new_process->euid = current_process ? current_process->euid : 0;
    new_process->suid = current_process ? current_process->suid : 0;
    new_process->egid = current_process ? current_process->egid : 0;
    new_process->sgid = current_process ? current_process->sgid : 0;
    new_process->priority = PRIORITY_DEFAULT;
    new_process->parent = current_process;
    new_process->threads = array_create();
    if(!new_process->threads) {
        free(new_process);
        goto release_and_fail;
    }

    new_process->children = array_create();
    if(!new_process->children) {
        free(new_process->threads);
        free(new_process);
        goto release_and_fail;
    }

    if(current_process) {
        if(!array_push(current_process->children, (u64) new_process)) {
            free(new_process->children);
            free(new_process->threads);
            free(new_process);
            goto release_and_fail;
        }
    }

    hashmap_put(process_map, (u64) pid, (u64) new_process);
    arch_spinlock_release(&process_map_lock);
    return pid;

release_and_fail:
    arch_spinlock_release(&process_map_lock);
    return -1;
}

pid_t thread_create(Process *process, int user, void (*start)(void *), void *arg) {
    if(!process || !start) {
        return -1;
    }

    arch_spinlock_acquire(&thread_map_lock);

    pid_t tid;
    if(!process->threads->count) {
        tid = process->pid;
    } else {
        tid = allocate_pid();
        if(tid < 0) {
            goto release_and_fail;
        }
    }

    Thread *new_thread = calloc(1, sizeof(Thread));
    if(!new_thread) {
        goto release_and_fail;
    }

    new_thread->tid = tid;
    new_thread->status = THREAD_STATUS_READY;
    new_thread->process = process;

    int new_context = process->page_tables == 0;
    new_thread->context = arch_create_context(user, start, arg,
                                              &new_thread->kernel_stack,
                                              &new_thread->user_stack,
                                              new_context ? &process->page_tables : NULL);

    if(!new_thread->context) {
        free(new_thread);
        goto release_and_fail;
    }

    if(new_context) {
        vmm_create_vaspace(&new_thread->process->vas, process->page_tables);
    }

    if(array_push(process->threads, (u64) new_thread)) {
        free(new_thread);
        goto release_and_fail;
    }

    CPUInfo *cpu_info = arch_get_current_cpu_info();
    CLDeque *queue = cpu_info->scheduler_state.ready_queues[process->priority];
    if(cldeque_push(queue, (u64) new_thread)) {
        array_pop_back(process->threads, NULL);
        free(new_thread);
        goto release_and_fail;
    }

    hashmap_put(thread_map, (u64) tid, (u64) new_thread);
    arch_spinlock_release(&thread_map_lock);
    return tid;

release_and_fail:
    arch_spinlock_release(&thread_map_lock);
    return -1;
}

Thread *find_next_thread(SchedulerState *state) {
    if(!state) {
        return NULL;
    }

    Thread *next_thread = NULL;
    for(int i = PRIORITY_MAX; i >= PRIORITY_MIN; i--) {
        if(!cldeque_steal(state->ready_queues[i], (u64 *) &next_thread)) {
            if(next_thread) {
                return next_thread;
            }
        }
    }
    return NULL;
}

void scheduler_tick(MachineContext *current_context) {
    if(!scheduler_enabled) {
        return;
    }
    CPUInfo *current_cpu_info = arch_get_current_cpu_info();
    if(!current_cpu_info) {
        return;
    }

    SchedulerState *state = &current_cpu_info->scheduler_state;
    Thread *current_thread = state->current_thread;
    Thread *next_thread = find_next_thread(state);

    if(!next_thread) {
        if(arch_get_cpu_count() == 1) {
            return;
        }

        CPUInfo *other_cpu_info;
        for(int i = 0; i < arch_get_cpu_count(); i++) {
            if(i == current_cpu_info->index) {
                continue;
            }

            other_cpu_info = arch_get_cpu_info(i);
            if(!other_cpu_info) {
                continue;
            }

            next_thread = find_next_thread(&other_cpu_info->scheduler_state);
            if(next_thread) {
                break;
            }
        }

        if(!next_thread) {
            return;
        }
    }

    current_cpu_info->scheduler_state.current_thread = next_thread;
    current_cpu_info->scheduler_state.current_process = next_thread->process;
    next_thread->status = THREAD_STATUS_RUNNING;
    if(current_thread) {
        current_thread->status = THREAD_STATUS_READY;
        arch_save_context(current_thread->context, current_context);
        if(cldeque_push(state->ready_queues[PRIORITY_DEFAULT], (u64) current_thread)) {
            return;
        }
    }

    arch_switch_context(next_thread->context, next_thread->process->page_tables);
}
