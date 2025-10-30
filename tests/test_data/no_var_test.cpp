#include <cstdio>
#include <unistd.h>
#include <cstdint>



int main() {
    int a = 42;
    for (int i = 0; i < 10; ++i) {
        uint64_t v = a;
        (void)v;
        a = a + 1;
    }
    return 0;
}

