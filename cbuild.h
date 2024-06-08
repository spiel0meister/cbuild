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

// Returns true if path1 was modified after path2
bool is_path_modified_after(const char* path1, const char* path2);

// Returns the provided path with the specified extension. "." MUST be specified by user
char* path_with_ext(const char* path, const char* ext);

// Returns true if the source files were modified after the target file. The srcs array MUST be NULL terminated
bool need_rebuild(const char* target, const char** srcs);
#define SRCS(...) ((const char*[]) { __VA_ARGS__, NULL })

// Rebuild the build program
void build_yourself_(Cmd* cmd, const char** cflags, size_t cflags_count, const char* src, const char* program);
#define build_yourself(cmd, argc, argv) assert(*(argc) >= 1); build_yourself_(cmd, NULL, 0, __FILE__, **(argv))
#define build_yourself_cflags(cmd, argc, argv, ...) do { \
        assert(*(argc) >= 1); \
        const char* cflags[] = { __VA_ARGS__ }; \
        size_t count = sizeof(cflags)/sizeof(cflags[0]); \
        build_yourself_(cmd, cflags, count, __FILE__, **(argv)); \
    } while (0)

// Resize a CMD.
void cmd_resize(Cmd* cmd);

// Creates a directory if it doesn't exist
bool create_dir_if_not_exists(const char* path);

// Returns if a string is safe as a shell argument
bool is_shell_safe(const char* str);
// Pushes strings to a CMD
void cmd_push_str_(Cmd* cmd, ...);
#define cmd_push_str(cmd, ...) cmd_push_str_(cmd, __VA_ARGS__, NULL)

// Runs the cmd and returns the pid of the process
Pid cmd_run_async(Cmd* cmd, bool log_cmd);
// Waits for a process to exit
bool pid_wait(Pid pid);
// Runs the cmd and returns if it was successful
bool cmd_run_sync(Cmd* cmd, bool log_cmd);

// Displays a CMD to stdout
void cmd_display(Cmd* cmd);

#define CMD(out, ...) do { \
        const char* args[] = { __VA_ARGS__, NULL }; \
        size_t len = sizeof(args)/sizeof(args[0]); \
        Cmd __cmd = { .items = args, .count = len }; \
        if ((out) != NULL) *(out) = cmd_run_sync(&__cmd); \
    } while (0)

#ifndef CBUILD_MALLOC
    #define CBUILD_MALLOC malloc
#endif // CBUILD_MALLOC

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

void cmd_resize(Cmd* cmd) {
    cmd->capacity = cmd->capacity == 0? 2 : cmd->capacity * 2;
    cmd->items = realloc(cmd->items, sizeof(*cmd->items) * cmd->capacity);
    assert(cmd->items);
}

bool is_path_modified_after(const char* path1, const char* path2) {
    struct stat st1, st2;
    if (stat(path1, &st1) == 0 && stat(path2, &st2) == 0) {
        time_t ctime1 = st1.st_mtime; 
        time_t ctime2 = st2.st_mtime; 

        if (difftime(ctime2, ctime1) < 0) {
            return true;
        }
    } else {
        return true;
    }

    return false;
}

char* path_with_ext(const char* path, const char* ext) {
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    const char* dot = strrchr(path, '.');
    if (dot == NULL) {
        char* out = CBUILD_MALLOC(path_len + ext_len + 1);
        memcpy(out, path, path_len);
        memcpy(out + path_len, ext, ext_len);
        out[path_len + ext_len] = 0;
        return out;
    } else {
        int pre_dot_len = dot - path;
        char* out = CBUILD_MALLOC(pre_dot_len + ext_len + 1);
        memcpy(out, path, pre_dot_len);
        memcpy(out + pre_dot_len, ext, ext_len);
        out[pre_dot_len + ext_len] = 0;
        return out;
    }
}

bool need_rebuild(const char* target, const char** srcs) {
    if (srcs == NULL) return true;

    for (int i = 0; srcs[i] != NULL; ++i) {
        if (is_path_modified_after(srcs[i], target)) return true;
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
                printf("[INFO] renamed %s to %s\n", TMP_FILE_NAME, program);
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

bool create_dir_if_not_exists(const char* path) {
    if (mkdir(path, 0775) == -1) {
        switch (errno) {
            case EEXIST:
                printf("[INFO] %s already exists\n", path);
                return true;
            default:
                printf("[ERROR] %s\n", strerror(errno));
                return false;
        }
    }
    
    printf("[INFO] created %s\n", path);
    return true;
}

void cmd_push_str_(Cmd* cmd, ...) {
    va_list args;
    va_start(args, cmd);
    char* arg = va_arg(args, char*);
    while (arg != NULL) {
        if (cmd->count == cmd->capacity) {
            cmd_resize(cmd);
        }
        cmd->items[cmd->count++] = arg;
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
        if (cmd->count == cmd->capacity) cmd_resize(cmd);
        if (cmd->items[cmd->count - 1] != NULL) {
            cmd->items[cmd->count++] = NULL;
        }
        if (execvp(cmd->items[0], cmd->items) != 0) {
            fprintf(stderr, "[DEBUG] %s\n", cmd->items[0]);
            fprintf(stderr, "[ERROR] couldn't execute command: %s\n", strerror(errno));
            exit(1);
        }
        assert(0 && "unreachable");
    }
    if (cmd->items[cmd->count - 2] != NULL) {
        cmd->count--;
    }

    return pid;
}

bool pid_wait(int pid) {
    while (1) {
        int wstatus = 0;
        if (waitpid(pid, &wstatus, 0) < 0) {
            fprintf(stderr, "[ERROR] could not wait on command (pid %d): %s\n", pid, strerror(errno));
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                fprintf(stderr, "[ERROR] command exited with exit code %d\n", exit_status);
                return false;
            }

            break;
        }

        if (WIFSIGNALED(wstatus)) {
            fprintf(stderr, "[ERROR] command process was terminated\n");
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
        if (!is_shell_safe(cmd->items[i])) {
            printf("'%s' ", cmd->items[i]);
        } else {
            printf("%s ", cmd->items[i]);
        }
        if (i == cmd->count - 1) {
            printf("\n");
        }
    }
}
#endif // CBUILD_IMPLEMENTATION
