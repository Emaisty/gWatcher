#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

static std::string run_command_capture_stdout(const std::string &cmd) {
    std::array<char, 128> buf;
    std::string result;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);
    return result;
}

std::pair<int, int> getReadsAndWrites(std::string &&out) {
    std::pair<int, int> res;
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.find("\twrite\t") != std::string::npos)
            ++res.first;
        if (line.find("\tread\t") != std::string::npos)
            ++res.second;
    }
    return res;
}

TEST(GWatchFunctional, BasicReadsAndWrites) { {
        std::string cmd = "g++ -O0 -g -o /tmp/basic_test.out test_data/basic_test.cpp";
        assert(system(cmd.c_str()) == 0);
    }

    std::string cmd = "./gwatch --var watched --exec /tmp/basic_test.out";

    auto res = getReadsAndWrites(run_command_capture_stdout(cmd));

    EXPECT_EQ(res.first, 11);
    EXPECT_EQ(res.second, 20);
}

TEST(GWatchFunctional, Functions) { {
        std::string cmd = "g++ -O0 -g -o /tmp/recursion_test.out test_data/recursion_test.cpp";
        assert(system(cmd.c_str()) == 0);
    }

    std::string cmd = "./gwatch --var watched --exec /tmp/recursion_test.out";

    auto res = getReadsAndWrites(run_command_capture_stdout(cmd));

    EXPECT_EQ(res.first, 21);
    EXPECT_EQ(res.second, 21);
}

TEST(GWatchFunctional, Arguments) {
    srand(time(NULL));
    std::string cmd = "g++ -O0 -g -o /tmp/args_test.out test_data/args_test.cpp";
    assert(system(cmd.c_str()) == 0);
    for (auto i = 0; i < 100; ++i) {
        int num = rand() % 100;
        std::string args = std::to_string(num);
        for (auto j = 0; j < num; ++j)
            args += " " + std::to_string(j);
        args += " 0";

        cmd = "/tmp/args_test.out " + args;
        assert(system(cmd.c_str()) == 0);
    }
}


static std::string write_test_target_and_compile(std::string &&body) {
    std::string header = R"cpp(
#include <cstdio>
#include <unistd.h>
#include <cstdint>

volatile uint64_t watched = 0;

int main() {
    int a;

)cpp";

    std::string footer = R"cpp(
    return 0;
}
)cpp";


    std::string cpath = "test_data/random_test.cpp";
    std::string binpath = "/tmp/random_test.out"; {
        std::ofstream f(cpath);
        f << (header + body + footer);
    }
    std::string cmd = "g++ -O0 -g -o " + binpath + " " + cpath;
    int rc = system(cmd.c_str());
    if (rc != 0)
        return "";
    return binpath;
}

std::string generate_if_statement(int &writes, int &reads) {
    std::string res = R"cpp()cpp";
    auto loop_num = rand() % 20;

    res += R"cpp(for(int i = 0; i < )cpp" + std::to_string(loop_num) + R"cpp(; ++i) {)cpp";

    for (int i = 0; i < loop_num; ++i) {
        int statement = rand() % 9;
        if (statement >= 0 && statement <= 2) {
            res += R"cpp(a = watched;)cpp";
            reads += loop_num;
        } else if (statement >= 3 && statement <= 5) {
            res += R"cpp(watched = 1;)cpp";
            writes += loop_num;
        } else {
            res += R"cpp(watched = watched + 1;)cpp";
            reads += loop_num, writes += loop_num;
        }
    }

    res += R"cpp(})cpp";
    return res;
}

TEST(GWatchFunctional, RandomTests) {
    srand(time(NULL));
    for (int i = 0; i < 20; ++i) {
        std::string body = R"cpp()cpp";
        int writes = 0, reads = 0;


        int statements_num = rand() % 60;

        for (int j = 0; j < statements_num; ++j) {
            int statement = rand() % 10;
            if (statement == 0) {
                body += generate_if_statement(writes, reads);
            } else if (statement >= 1 && statement <= 3) {
                body += R"cpp(a = watched;)cpp";
                reads++;
            } else if (statement >= 4 && statement <= 6) {
                body += R"cpp(watched = 1;)cpp";
                writes++;
            } else {
                body += R"cpp(watched = watched + 1;)cpp";
                reads++, writes++;
            }
        }

        std::string bin = write_test_target_and_compile(std::move(body));
        ASSERT_FALSE(bin.empty());

        std::string cmd = "./gwatch --var watched --exec " + bin;

        auto res = getReadsAndWrites(run_command_capture_stdout(cmd));
        EXPECT_EQ(res.first, writes);
        EXPECT_EQ(res.second, reads);
    }
}
