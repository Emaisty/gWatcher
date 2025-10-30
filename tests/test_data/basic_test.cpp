#include <cstdio>
#include <unistd.h>
#include <cstdint>

volatile uint64_t watched = 0;

int main() {

    watched = 42;
    for (int i = 0; i < 10; ++i) {
        uint64_t v = watched;
        (void)v;
        watched = watched + 1;
    }
    return 0;
}

