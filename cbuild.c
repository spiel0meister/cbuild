#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#define EXAMPLE_DIR "example"

int main(int argc, char** argv) {
    Cmd cmd = {};
    build_yourself_cflags(&cmd, &argc, &argv, "-std=c2x", "-Wall", "-Wextra");

    const char* target = EXAMPLE_DIR"/one";
    if (need_rebuild(target, SRCS(path_with_ext(target, ".c")))) {
        cmd_push_str(&cmd, "gcc", "-o", target, EXAMPLE_DIR"/one.c");
        if (!cmd_run_sync(&cmd, true)) return 1;
    }

    return 0;
}
