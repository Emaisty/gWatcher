#include <cstdio>
#include <unistd.h>
#include <cstdint>

volatile uint64_t watched = 0;


void writes(int n) {
    watched = n;
    if (n > 0)
        writes(n - 1);
}

void reads(int n) {
    int tmp = watched;
    if (n > 0)
        reads(n - 1);
}


int main() {
    writes(20);
    reads(20);
    return 0;
}
