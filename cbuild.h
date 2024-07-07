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

#ifndef PIDS_INIT_CAP
#define PIDS_INIT_CAP 128
#endif // PIDS_INIT_CAP

typedef struct {
    Pid* items;
    size_t count;
    size_t capacity;
}Pids;

typedef struct {
    char name[256];
    char path[1024];
}File;

typedef struct {
    File* items;
    size_t count;
    size_t capacity;
}Files;

// Returns true if path1 was modified after path2
bool is_path_modified_after(const char* path1, const char* path2);

// Returns the provided path with the specified extension. "." MUST be specified by user
char* path_with_ext(const char* path, const char* ext);

// Returns true if the source files were modified after the target file. The srcs array MUST be NULL terminated
bool need_rebuild(const char* target, Files* srcs);
#define need_rebuild1(target, src) is_path_modified_after(src, target)

// Rebuild the build program
void build_yourself_(Cmd* cmd, const char** cflags, size_t cflags_count, const char* src, int argc, char** argv);
#define build_yourself(cmd, argc, argv) assert(argc >= 1); build_yourself_(cmd, NULL, 0, __FILE__, argc, argv)
#define build_yourself_cflags(cmd, argc, argv, ...) do { \
        assert(argc >= 1); \
        const char* cflags[] = { __VA_ARGS__ }; \
        size_t count = sizeof(cflags)/sizeof(cflags[0]); \
        build_yourself_(cmd, cflags, count, __FILE__, argc, argv); \
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
Pid cmd_run_async(Cmd* cmd);
// Waits for a process to exit
bool pid_wait(Pid pid);

// Resizes a Pids to fit the specified count
void pids_maybe_resize(Pids* pids, size_t count);
// Appends a Pid to Pids
void pids_append(Pids* pids, Pid pid);
// Appends many Pids to Pids
void pids_append_many(Pids* pids, Pid* pid_items, size_t pid_count);
// Waits for all Pids and returns if all were successful
bool pids_wait(Pids* pids);

// Resizes a Files to fit the specified count
void files_maybe_resize(Files* files, size_t count);
// Appends a File to Files
void files_append(Files* files, File file);
// Appends many Files to Files
void files_append_many(Files* files, File* file_items, size_t file_count);

void files_list_null(Files* files, ...);
#define files_list(files, ...) files_list_null(files, __VA_ARGS__, NULL)

void dir_collect_files(Files* files, const char* dirpath, const char* ext, bool recursive);

// Runs the cmd and returns if it was successful
bool cmd_run_sync(Cmd* cmd);

// Displays a CMD to stdout
void cmd_display(Cmd* cmd);

#define CMD(out, ...) do { \
        const char* args[] = { __VA_ARGS__, NULL }; \
        size_t len = sizeof(args)/sizeof(args[0]); \
        Cmd __cmd = { .items = args, .count = len }; \
        if ((out) != NULL) *(out) = cmd_run_sync(&__cmd); \
        else cmd_run_sync(&__cmd); \
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
    #include <dirent.h>
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

bool need_rebuild(const char* target, Files* srcs) {
    if (srcs == NULL) return true;

    for (int i = 0; i < srcs->count; ++i) {
        if (is_path_modified_after(srcs->items[i].path, target)) return true;
    }

    return false;
}

#define TMP_FILE_NAME "./tmp"
void build_yourself_(Cmd* cmd, const char** cflags, size_t cflags_count, const char* src, int argc, char** argv) {
    const char* program = *argv++; argc--;
    if (is_path_modified_after(src, program)) {
        cmd_push_str(cmd, "mv", program, TMP_FILE_NAME);
        if (!cmd_run_sync(cmd)) { 
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

        if (!cmd_run_sync(cmd)) {
            cmd->count = 0;
            cmd_push_str(cmd, "mv", TMP_FILE_NAME, program);
            if (!cmd_run_sync(cmd)) {
                fprintf(stderr, "[WARN] failed to rename %s to %s\n", TMP_FILE_NAME, program);
            } else {
                printf("[INFO] renamed %s to %s\n", TMP_FILE_NAME, program);
            }
            abort();
        } else {
            cmd->count = 0;
            cmd_push_str(cmd, "rm", TMP_FILE_NAME);
            if (!cmd_run_sync(cmd)) {
                fprintf(stderr, "[WARN] failed to delete %s\n", TMP_FILE_NAME);
            } else {
                printf("[INFO] deleted %s\n", TMP_FILE_NAME);
            }
        }

        cmd->count = 0;
        cmd_push_str(cmd, program);
        for (int i = 0; i < argc; ++i) {
            cmd_push_str(cmd, argv[i]);
        }
        cmd_run_sync(cmd);
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

int cmd_run_async(Cmd* cmd) {
    printf("[CMD] ");
    cmd_display(cmd);

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

void pids_maybe_resize(Pids* pids, size_t count) {
    if (pids->count + count >= pids->capacity) {
        if (pids->capacity == 0) pids->capacity = PIDS_INIT_CAP;
        while (pids->count + count >= pids->capacity) {
            pids->capacity *= 2;
        } 
        pids->items = realloc(pids->items, sizeof(pids->items[0]) * pids->capacity);
    }
}

void pids_append(Pids* pids, Pid pid) {
    pids_maybe_resize(pids, 1);
    pids->items[pids->count++] = pid;
}

void pids_append_many(Pids* pids, Pid* pid_items, size_t pid_count) {
    pids_maybe_resize(pids, pid_count);
    for (int i = 0; (size_t)i < pid_count; ++i) {
        pids->items[pids->count++] = pid_items[i];
    }
}

bool pids_wait(Pids* pids) {
    bool success = true;
    for (size_t i = 0; i < pids->count; ++i) {
        if (!pid_wait(pids->items[i])) success = false;
    }
    return success;
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

void files_maybe_resize(Files* files, size_t count) {
    if (files->count + count >= files->capacity) {
        if (files->capacity == 0) files->capacity = PIDS_INIT_CAP;
        while (files->count + count >= files->capacity) {
            files->capacity *= 2;
        } 
        files->items = realloc(files->items, sizeof(files->items[0]) * files->capacity);
    }
}

void files_append(Files* files, File file) {
    files_maybe_resize(files, 1);
    files->items[files->count++] = file;
}

void files_append_many(Files* files, File* file_items, size_t file_count) {
    files_maybe_resize(files, file_count);
    for (int i = 0; (size_t)i < file_count; ++i) {
        files->items[files->count++] = file_items[i];
    }
}

void files_list_null(Files* files, ...) {
    va_list args;
    va_start(args, files);

    const char* filepath = va_arg(args, const char*);
    for (; filepath != NULL; filepath = va_arg(args, const char*)) {
        File file = {0};
        char* as = strrchr(filepath, '/');

        if (as == NULL) {
            snprintf(file.name, 256, "%s", filepath);
        } else {
            snprintf(file.name, 256, "%s", as + 1);
        }

        snprintf(file.path, 1024, "%s", filepath);
        files_append(files, file);
    }

    va_end(args);
}
#define files_list(files, ...) files_list_null(files, __VA_ARGS__, NULL)

void dir_collect_files(Files* files, const char* dirpath, const char* ext, bool recursive) {
    DIR* dirs[1024] = {0};
    char dir_paths[1024][1024] = {0};
    size_t dir_count = 0;

    dirs[dir_count] = opendir(dirpath);
    snprintf(dir_paths[dir_count++], 1024, "%s", dirpath);
dir_while:while (dir_count > 0) {
        DIR* dir = dirs[dir_count - 1];
        char* dirpath = dir_paths[dir_count - 1];

        if (dir == NULL) {
            dir_count--;
            continue;
        }

        struct dirent* ent = readdir(dir); 
        for (; ent != NULL; ent = readdir(dir)) {
            if (ent->d_type == DT_DIR) {
                if (!recursive) continue;

                dirs[dir_count] = opendir(ent->d_name);
                snprintf(dir_paths[dir_count++], 1024, "%s/%s", dirpath, ent->d_name);
                goto dir_while;
            }

            if (ext != NULL) {
                char* dot = strchr(ent->d_name, '.');
                if (dot != NULL && strcmp(dot, ext) != 0) continue;
            }

            File file = {0};
            memcpy(file.name, ent->d_name, 256);
            snprintf(file.path, 1024, "%s/%s", dirpath, ent->d_name);

            files_append(files, file);
        }

        closedir(dir);
        dir_count--;
    }
}

bool cmd_run_sync(Cmd* cmd) {
    int pid = cmd_run_async(cmd);
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
