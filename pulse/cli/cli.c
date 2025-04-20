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
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <limits.h>

static int __signaled = 0;
static const char *__name;

char *__image_name = NULL;
FILE *__disk;
u16 __block_size = DEFAULT_BLOCK_SIZE;
u8 __fanout = DEFAULT_FANOUT_FACTOR;
u64 __block_count = 0;

#define PROMPT (ESC_BOLD_CYAN "⌘" ESC_RESET " ")

void print_prompt(const char *name, int status) {
    if(status) {
        fputs(ESC_BOLD_RED "✗" ESC_RESET, stdout);
    } else {
        fputs(ESC_BOLD_GREEN "✓" ESC_RESET, stdout);
    }

    if(__image_name) printf(" " ESC_BOLD_BLUE "%s" ESC_RESET, __image_name);

    putchar(' ');
    fputs(PROMPT, stdout);
    fflush(stdout);
}

void sigint_handler(int sig) {
    if(__signaled) {
        printf(ESC_RESET "\n");
        exit(0);
    }
    __signaled = 1;
    printf(ESC_BOLD_YELLOW "\npress ctrl+c again to quit." ESC_RESET "\n\n");
    print_prompt(__name, 1);
}

int exit_command(int argc, char **argv);
int help_command(int argc, char **argv);

struct Command commands[] = {
    {"exit", "exit the command line interface", exit_command},
    {"help", "show this help message", help_command},
    {"mount", "mount a disk image", mount_command},
    {"umount", "unmount a disk image", NULL},
    {"create", "create a new disk image", create_command},
    {"format", "format a disk image", NULL},
    {"info", "show information about a mounted image", NULL},
    {"sync", "sync the file system to the disk image", NULL},
    {"check", "check the file system for errors", NULL},
    {"repair", "repair the file system", NULL},
};

int exit_command(int argc, char **argv) {
    exit(0);
}

int help_command(int argc, char **argv) {
    usize longest = 0;
    for(int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        usize len = strlen(commands[i].name);
        if(len > longest) {
            longest = len;
        }
    }
    longest += 2;

    printf(" ⚙️  available commands:\n");
    for(int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        printf("   " ESC_BOLD "%s" ESC_RESET, commands[i].name);
        for(usize j = strlen(commands[i].name); j < longest; j++) {
            putchar(' ');
        }
        printf("%s\n", commands[i].description);
    }
    return 0;
}

static inline int min(int a, int b, int c) {
    if(a < b && a < c) return a;
    if(b < a && b < c) return b;
    return c;
}

static int levenshtein(const char *s1, const char *s2) {
    usize len1 = strlen(s1);
    usize len2 = strlen(s2);

    int **d = malloc((len1 + 1) * sizeof(int *));
    if(!d) {
        perror("malloc");
        return -1;
    }

    for(usize i = 0; i <= len1; i++) {
        d[i] = malloc((len2 + 1) * sizeof(int));
        if(!d[i]) {
            perror("malloc");
            for(usize j = 0; j < i; j++) {
                free(d[j]);
            }
            free(d);
            return -1;
        }
    }

    for(usize i = 0; i <= len1; i++) {
        d[i][0] = i;
    }

    for(usize j = 0; j <= len2; j++) {
        d[0][j] = j;
    }

    for(usize i = 1; i <= len1; i++) {
        for(usize j = 1; j <= len2; j++) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = min(
                d[i - 1][j] + 1,
                d[i][j - 1] + 1,
                d[i - 1][j - 1] + cost
            );
        }
    }

    int dist = d[len1][len2];
    for(usize i = 0; i <= len1; i++) {
        free(d[i]);
    }
    free(d);
    return dist;
}

void not_found(const char *command) {
    const char *closest = NULL;
    int min_dist = INT_MAX;

    for(int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        int dist = levenshtein(command, commands[i].name);
        if(dist < min_dist) {
            min_dist = dist;
            closest = commands[i].name;
        }
    }

    if(min_dist <= 2 && closest) {
        printf(ESC_BOLD_RED "%s:" ESC_RESET " command not found, did you mean '%s'?\n", command, closest);
        return;
    }

    printf(ESC_BOLD_RED "%s:" ESC_RESET " command not found\n", command);
}

int command_line(const char *name) {
    __name = name;
    printf(ESC_RESET "pulse command-line interface\n");

    signal(SIGINT, sigint_handler);

    printf("🌍 https://jewelcodes.io/pulse\n");
    printf("❓ type 'help' for a list of commands.\n\n");
    printf(ESC_BOLD_GREEN "💡 tip: " ESC_RESET "start by mounting a disk image or creating one.\n\n");

    int status = 0;
    int first_run = 1;

    for(;;) {
        char *line = NULL;
        usize len = 0;
        ssize read;

        if(first_run) {
            first_run = 0;
            printf(PROMPT);
        } else {
            print_prompt(name, status);
        }

        read = getline(&line, &len, stdin);
        __signaled = 0;
        if(read == -1) {
            perror("getline");
            free(line);
            return 1;
        }

        if(line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        char *args[8];
        int argc = 0;

        char *token = strtok(line, " ");
        while(token != NULL && argc < sizeof(args) / sizeof(args[0])) {
            args[argc++] = token;
            token = strtok(NULL, " ");
        }

        if(!argc) {
            free(line);
            continue;
        }

        for(int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
            if(!strcmp(args[0], commands[i].name)) {
                if(commands[i].function) {
                    status = commands[i].function(argc, args);
                    break;
                } else {
                    printf(ESC_BOLD_RED "%s:" ESC_RESET " unimplemented function\n", commands[i].name);
                    status = 1;
                    break;
                }
            } else {
                if(i == ((sizeof(commands)-1) / sizeof(commands[0]))) {
                    not_found(args[0]);
                    status = 1;
                    break;
                }
            }
        }

        free(line);
    }

    return 0;
}

int script(int argc, char **argv) {
    for(int i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if(!strcmp(argv[1], commands[i].name)) {
            if(commands[i].function) {
                return commands[i].function(argc-1, argv+1);
            } else {
                printf(ESC_BOLD_RED "%s:" ESC_RESET " unimplemented function\n", commands[i].name);
                return 1;
            }
        }
    }

    not_found(argv[1]);
    return 1;
}