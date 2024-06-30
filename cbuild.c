#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

int main(int argc, char** argv) {
    const char* cflags[] = { "-std=c2x", "-Wall", "-Wextra", NULL };
    Cmd cmd = {};
    build_yourself_cflags(&cmd, argc, argv);

    return 0;
}
