#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

int main(int argc, char** argv) {
    Cmd cmd = {};
    build_yourself_cflags(&cmd, argc, argv);

    Files files = {0};
    files_list(&files, "example/one.c");

    if (true || need_rebuild("example/one", &files)) {
        cmd_push_str(&cmd, "gcc", "-Wall", "-Wextra", "-o", "example/one", "example/one.c");
        if (!cmd_run_sync(&cmd)) return 1;
        cmd.count = 0;
    }

    return 0;
}
