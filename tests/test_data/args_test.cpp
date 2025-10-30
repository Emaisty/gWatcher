#include <cstdio>
#include <unistd.h>
#include <cstdint>
#include <string>

volatile uint64_t watched = 0;

int main(int argc, char **argv) {
    if (argc < 3)
        return 1;
    int num = std::stoi(argv[1]);
    if (num + 3 != argc)
        return 1;
    for (int i = 0; i < num + 1; ++i) {
        std::string str = argv[i + 2];
        if (i == num) {
            if ("0" != str)
                return 1;
        } else {
            if (std::to_string(i) != str)
                return 1;
        }
    }
    return 0;
}
