#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#define EXAMPLE_DIR "example"

int main(int argc, char** argv) {
    const char* cflags[] = { "-std=c2x", "-Wall", "-Wextra", NULL };
    Cmd cmd = {};
    build_yourself_cflags(&cmd, argc, argv, "-std=c2x", "-Wall", "-Wextra");

    if (!cmd_build_c(&cmd, CC_GCC, EXAMPLE_DIR"/one", STRS(path_with_ext(EXAMPLE_DIR"/one", ".c")), cflags)) return 1;

    return 0;
}
