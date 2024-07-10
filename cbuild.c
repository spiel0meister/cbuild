#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#include <unistd.h>

void watch(Cmd* cmd) {
    cmd->count = 0;
    Files srcs = {0};
    files_list(&srcs, "example/one.c");
    for(; 1; usleep(1 / 10)) {
        size_t build_count = 0;

        if (need_rebuild("example/one", &srcs)) {
            cmd_push_str(cmd, "gcc", "-Wall", "-Wextra", "-o", "example/one", "example/one.c");
            if (!cmd_run_sync(cmd)) exit(1);
            cmd->count = 0;
            build_count++;
        }

        if (build_count > 0) {
            printf("---------\n");
        }
    }
}

int main(int argc, char** argv) {
    Cmd cmd = {};
    build_yourself_cflags(&cmd, argc, argv);

    const char* program = pop_argv(&argc, &argv);

    if (argc > 0) {
        const char* subcmd = pop_argv(&argc, &argv);

        if (strcmp(subcmd, "watch") == 0) {
            watch(&cmd);
        } else {
            fprintf(stderr, "Unknown subcommand %s\n", subcmd);
            return 1;
        }
    }

    Files srcs = {0};
    files_list(&srcs, "example/one.c");

    if (need_rebuild("example/one", &srcs)) {
        cmd_push_str(&cmd, "gcc", "-Wall", "-Wextra", "-o", "example/one", "example/one.c");
        if (!cmd_run_sync(&cmd)) return 1;
        cmd.count = 0;
    }

    return 0;
}
