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

#include <sys/types.h>
#include <kiwi/structs/array.h>
#include <kiwi/structs/cldeque.h>

#define MAX_PROCESSES                   65536

#define PRIORITY_MIN                    0
#define PRIORITY_MAX                    5
#define PRIORITY_COUNT                  (PRIORITY_MAX - PRIORITY_MIN + 1)
#define PRIORITY_DEFAULT                (PRIORITY_COUNT / 2)

#define THREAD_STATUS_READY             1
#define THREAD_STATUS_RUNNING           2
#define THREAD_STATUS_BLOCKED           3
#define THREAD_STATUS_TERMINATED        4

typedef struct Thread Thread;
typedef struct Process Process;

struct Thread {
    pid_t tid;
    int status;
    Process *process;
    void *context;      /* arch-specific */
    uptr kernel_stack;
    uptr user_stack;
};

struct Process {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    uid_t euid;
    uid_t suid;
    gid_t egid;
    gid_t sgid;
    unsigned int priority;
    uptr page_tables;
    Process *parent;
    Array *threads;
    Array *children;
};

typedef struct SchedulerState {
    CLDeque *ready_queues[PRIORITY_COUNT];
    Process *current_process;
    Thread *current_thread;
    Thread *idle_thread;
} SchedulerState;

void scheduler_init(void);
void scheduler_start(void);
void scheduler_stop(void);
void scheduler_tick(void);
void sched_yield(void);
pid_t getpid(void);
pid_t gettid(void);
Process *get_current_process(void);
Thread *get_current_thread(void);
pid_t process_create(void);
pid_t thread_create(Process *process, int user, void (*start)(void *), void *arg);
