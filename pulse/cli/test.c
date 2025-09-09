/*
 * pulse - a highly scalable SSD-first file system with predictable logarithmic
 * bounds across all operations
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

#include <pulse/pulse.h>
#include <pulse/cli.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

struct Test {
    const char *name;
    const char *description;
    int (*function)();
};

static int test_create() {
    mkdir("test", S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    char *args[] = { "create", "test/test.img", "2g" };

    int status = create_command(sizeof(args) / sizeof(args[0]), args);
    if(status) return status;
    return 0;
}

static int test_mount() {
    char *args[] = { "mount", "test/test.img" };

    int status = mount_command(sizeof(args) / sizeof(args[0]), args);
    if(status) return status;
    return 0;
}

static int test_allocate_blocks() {
    u64 block, expected, free_test = 0;
    srand(time(NULL));
    int test_count = mountpoint->fanout * 256;

    printf(ESC_BOLD_CYAN "test:" ESC_RESET " running %d allocation tests...\n", test_count);

    int random = rand() % test_count;

    for(int i = 0; i < test_count; i++) {
        block = allocate_block();
        if(block == -1) {
            printf(ESC_BOLD_RED "test:" ESC_RESET " failed to allocate block\n");
            return 1;
        }
        printf("    🛠️ allocated block %llu\n", block);

        if(i > 0 && block != expected) {
            printf(ESC_BOLD_RED "test:" ESC_RESET " allocated block %llu but expected %llu\n", block, expected);
            return 1;
        }

        expected = block + 1;

        if(i == random) {
            free_test = block; // we will try to free and reallocate this random block
        }
    }

    printf("    🛠️ attempt to free and reallocate block %llu\n", free_test);
    free_block(free_test);

    block = allocate_block();
    if(block == -1) {
        printf(ESC_BOLD_RED "test:" ESC_RESET " failed to allocate block\n");
        return 1;
    }

    if(block != free_test) {
        printf(ESC_BOLD_RED "test:" ESC_RESET " allocated block %llu but expected %llu\n", block, free_test);
        return 1;
    }

    return 0;
}

int test_dump_root() {
    return dump_inode(resolve("/"));
}

struct Test tests[] = {
    {"create", "creating new disk image", test_create},
    {"mount", "mounting disk image", test_mount},
    {"allocate", "allocating blocks", test_allocate_blocks},
    {"dumproot", "dumping root inode", test_dump_root},
};

int test_command(int argc, char **argv) {
    printf(ESC_BOLD_CYAN "test:" ESC_RESET " running tests...\n");

    int test_count = sizeof(tests) / sizeof(tests[0]);
    int fail_count = 0;

    for(int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        printf(ESC_BOLD_CYAN "test:" ESC_RESET " 🔄 running test %s - %s\n", tests[i].name, tests[i].description);
        if(tests[i].function) {
            int status = tests[i].function();
            if(status) {
                printf(ESC_BOLD_RED "test:" ESC_RESET " ⚠️ test %s failed\n", tests[i].name);
                fail_count++;
            } else {
                printf(ESC_BOLD_GREEN "test:" ESC_RESET " ✅ test %s passed\n", tests[i].name);
            }
        }
    }

    if(fail_count) {
        printf(ESC_BOLD_RED "test:" ESC_RESET " ❌ %d/%d test%s failed\n", fail_count, test_count,
            fail_count > 1 ? "s" : "");
    } else {
        printf(ESC_BOLD_GREEN "test:" ESC_RESET " ✅ %d/%d test%s passed\n", test_count, test_count,
            test_count > 1 ? "s" : "");
    }

    return fail_count ? 1 : 0;
}
