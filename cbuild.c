#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#include <unistd.h>

void watch(Cmd* cmd) {
    cmd->count = 0;
    for(;; usleep(100)) {
        size_t build_count = 0;

        if (need_rebuild1("example/one", "example/one.c")) {
            cmd_push_str(cmd, "gcc", "-Wall", "-Wextra", "-o", "example/one", "example/one.c");
            if (!cmd_run_sync_and_reset(cmd)) exit(1);
            build_count++;
        }

        if (build_count > 0) {
            printf("---------\n");
        }
    }
}

int main(int argc, char** argv) {
    Cmd cmd = {};
    build_yourself(&cmd, argc, argv);

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

    if (need_rebuild1("example/one", "example/one.c")) {
        cmd_push_str(&cmd, "gcc", "-Wall", "-Wextra", "-o", "example/one", "example/one.c");
        if (!cmd_run_sync_and_reset(&cmd)) return 1;
    }

    return 0;
}
