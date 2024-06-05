/*
 * Copyright 2024 Žan Sovič <soviczan7@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef CBUILD_H
#define CBUILD_H
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char** items;
    size_t count;
    size_t capacity;
}Cmd;

typedef int Pid;

bool is_path_modified_after(const char* path1, const char* path2);
void build_yourself_(Cmd* cmd, const char** cflags, size_t cflags_count, const char* src, const char* program);
#define build_yourself(cmd, program) build_yourself_(cmd, NULL, 0, __FILE__, program)
#define build_yourself_cflags(cmd, cflags, count, program) build_yourself_(cmd, cflags, count, __FILE__, program)

bool is_shell_safe(const char* str);
void cmd_push_str_(Cmd* cmd, ...);
#define cmd_push_str(cmd, ...) cmd_push_str_(cmd, __VA_ARGS__, NULL)

Pid cmd_run_async(Cmd* cmd, bool log_cmd);
bool pid_wait(Pid pid);
bool cmd_run_sync(Cmd* cmd, bool log_cmd);

void cmd_display(Cmd* cmd);

#endif // CBUILD_H

#ifdef CBUILD_IMPLEMENTATION
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <sys/wait.h>
    #include <sys/stat.h>
#else
    #error "niche videogame os not supported"
#endif

bool is_path_modified_after(const char* path1, const char* path2) {
    struct stat st1, st2;
    if (stat(path1, &st1) == 0 && stat(path2, &st2) == 0) {
        time_t ctime1 = st1.st_mtime; 
        time_t ctime2 = st2.st_mtime; 

        if (difftime(ctime2, ctime1) < 0) {
            return true;
        }
    }

    return false;
}

#define TMP_FILE_NAME "./tmp"
void build_yourself_(Cmd* cmd, const char** cflags, size_t cflags_count, const char* src, const char* program) {
    if (is_path_modified_after(src, program)) {
        cmd_push_str(cmd, "mv", program, TMP_FILE_NAME);
        if (!cmd_run_sync(cmd, false)) { 
            fprintf(stderr, "[ERROR] failed to rename %s to %s\n", program, TMP_FILE_NAME);
            abort();
        }
        cmd->count = 0;
        printf("[INFO] renamed %s to %s\n", program, TMP_FILE_NAME);

        cmd_push_str(cmd, "gcc");
        if (cflags_count > 0) {
            for (size_t i = 0; i < cflags_count; i++) {
                cmd_push_str(cmd, cflags[i]);
            }
        }
        cmd_push_str(cmd, "-o", program, src);

        if (!cmd_run_sync(cmd, true)) {
            cmd->count = 0;
            cmd_push_str(cmd, "mv", TMP_FILE_NAME, program);
            if (!cmd_run_sync(cmd, false)) {
                fprintf(stderr, "[WARN] failed to rename %s to %s\n", TMP_FILE_NAME, program);
            } else {
                printf("[WARN] renamed %s to %s\n", TMP_FILE_NAME, program);
            }
            abort();
        } else {
            cmd->count = 0;
            cmd_push_str(cmd, "rm", TMP_FILE_NAME);
            if (!cmd_run_sync(cmd, false)) {
                fprintf(stderr, "[WARN] failed to delete %s\n", TMP_FILE_NAME);
            } else {
                printf("[INFO] deleted %s\n", TMP_FILE_NAME);
            }
        }

        cmd->count = 0;
        cmd_push_str(cmd, program);
        cmd_run_sync(cmd, false);
        exit(0);
    }
}

bool is_shell_safe(const char* str) {
    while (str[0] != 0) {
        switch (str[0]) {
            case ' ':
                return false;
            default:
                str++;
        }
    }
    return true;
}

void cmd_push_str_(Cmd* cmd, ...) {
    va_list args;
    va_start(args, cmd);
    char* arg = va_arg(args, char*);
    while (arg != NULL) {
        if (cmd->count == cmd->capacity) {
            cmd->capacity = cmd->capacity == 0? 2 : cmd->capacity * 2;
            cmd->items = realloc(cmd->items, sizeof(*cmd->items) * cmd->capacity);
            assert(cmd->items);
        }
        if (!is_shell_safe(arg)) {
            size_t new_len = strlen(arg) + 3;
            char* cstr = malloc(new_len + 1);
            snprintf(cstr, new_len, "\"%s\"", arg);
            cstr[new_len] = 0;
            cmd->items[cmd->count++] = cstr;
        } else {
            cmd->items[cmd->count++] = arg;
        }
        arg = va_arg(args, char*);
    }
    va_end(args);
}

int cmd_run_async(Cmd* cmd, bool log_cmd) {
    if (log_cmd) {
        printf("[CMD] ");
        cmd_display(cmd);
    }

    int pid = fork();

    if (pid < 0) {
        fprintf(stderr, "[ERROR] couldn't start subprocces: %s\n", strerror(errno));
        exit(1);
    } else if (pid == 0) {
        cmd->items[cmd->count++] = NULL;
        if (execvp(cmd->items[0], cmd->items) != 0) {
            fprintf(stderr, "[DEBUG] %s\n", cmd->items[0]);
            fprintf(stderr, "[ERROR] couldn't execute command: %s\n", strerror(errno));
            exit(1);
        }
        assert(0 && "unreachable");
    }
    cmd->count--;

    return pid;
}

bool pid_wait(int pid) {
    while (1) {
        int wstatus = 0;
        if (waitpid(pid, &wstatus, 0) < 0) {
            fprintf(stderr, "[ERROR] could not wait on command (pid %d): %s", pid, strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                fprintf(stderr, "[ERROR] command exited with exit code %d", exit_status);
                return false;
            }

            break;
        }

        if (WIFSIGNALED(wstatus)) {
            fprintf(stderr, "[ERROR] command process was terminated");
            return false;
        }
    }

    return true;
}

bool cmd_run_sync(Cmd* cmd, bool log_cmd) {
    int pid = cmd_run_async(cmd, log_cmd);
    return pid_wait(pid);
}

void cmd_display(Cmd* cmd) {
    for (size_t i = 0; i < cmd->count; i++) {
        if (i == cmd->count - 1) {
            printf("%s\n", cmd->items[i]);
        } else {
            printf("%s ", cmd->items[i]);
        }
    }
}
#endif // CBUILD_IMPLEMENTATION
