#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include <elf.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <iomanip>

static void err_exit(const std::string &e, int code = 1) {
    std::cerr << e << std::endl;
    exit(code);
}

struct SymbolInfo {
    uint64_t value;
    uint64_t size;
    bool is_defined;
};

static std::optional<SymbolInfo> find_symbol_in_elf(const std::string &path, const std::string &symname) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0)
        return std::nullopt;

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return std::nullopt;
    }
    size_t filesize = st.st_size;

    std::vector<char> data(filesize);
    ssize_t r = read(fd, data.data(), filesize);
    close(fd);
    if (r != (ssize_t) filesize)
        return std::nullopt;

    const unsigned char *mem = reinterpret_cast<const unsigned char *>(data.data());

    if (filesize < sizeof(Elf64_Ehdr))
        return std::nullopt;
    const Elf64_Ehdr *eh = reinterpret_cast<const Elf64_Ehdr *>(mem);

    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0)
        return std::nullopt;

    if (eh->e_ident[EI_CLASS] != ELFCLASS64)
        return std::nullopt;


    const Elf64_Shdr *shdrs = reinterpret_cast<const Elf64_Shdr *>(mem + eh->e_shoff);
    const char *shstr = reinterpret_cast<const char *>(mem + shdrs[eh->e_shstrndx].sh_offset);

    auto read_symtab = [&](const Elf64_Shdr &symsh)-> std::optional<SymbolInfo> {
        const char *strtab = nullptr;
        if (symsh.sh_link < eh->e_shnum) {
            const Elf64_Shdr &strsh = shdrs[symsh.sh_link];
            strtab = reinterpret_cast<const char *>(mem + strsh.sh_offset);
        }
        const Elf64_Sym *syms = reinterpret_cast<const Elf64_Sym *>(mem + symsh.sh_offset);
        size_t n = symsh.sh_size / symsh.sh_entsize;
        for (size_t i = 0; i < n; ++i) {
            const Elf64_Sym &s = syms[i];
            if (s.st_name == 0) continue;
            const char *name = strtab + s.st_name;
            if (symname == name) {
                SymbolInfo info;
                info.value = s.st_value;
                info.size = s.st_size;
                info.is_defined = (s.st_shndx != SHN_UNDEF);
                return info;
            }
        }
        return std::nullopt;
    };

    for (int i = 0; i < eh->e_shnum; ++i) {
        const char *secname = shstr + shdrs[i].sh_name;
        if (strcmp(secname, ".symtab") == 0 || strcmp(secname, ".dynsym") == 0) {
            auto res = read_symtab(shdrs[i]);
            if (res)
                return res;
        }
    }
    return std::nullopt;
}

static std::optional<uint64_t> get_base_address_of_mapping(pid_t pid, const std::string &exe_path) {
    std::ostringstream oss;
    oss << "/proc/" << pid << "/maps";
    std::ifstream f(oss.str());
    if (!f)
        return
                std::nullopt;
    std::string line;
    int count = 0;
    while (getline(f, line)) {
        if (line.find(exe_path) != std::string::npos) {
            size_t dash = line.find('-');
            if (dash == std::string::npos)
                continue;
            std::string start_hex = line.substr(0, dash);
            uint64_t start = stoull(start_hex, nullptr, 16);
            return start;
        }
    }
    return std::nullopt;
}

static uint64_t ptrace_peek(pid_t pid, uint64_t addr) {
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, pid, (void *) addr, nullptr);
    if (word == -1 && errno)
        err_exit(std::string("ptrace PEEKDATA failed: ") + strerror(errno), 12);

    return (uint64_t) word;
}

static void ptrace_pokeuser(pid_t pid, unsigned reg_offset, uint64_t value) {
    if (ptrace(PTRACE_POKEUSER, pid, (void *) reg_offset, (void *) value) == -1) {
        err_exit(std::string("ptrace POKEUSER failed: ") + strerror(errno), 13);
    }
}

static uint64_t ptrace_peekuser(pid_t pid, unsigned reg_offset) {
    errno = 0;
    long val = ptrace(PTRACE_PEEKUSER, pid, (void *) reg_offset, nullptr);
    if (val == -1 && errno) {
        err_exit(std::string("ptrace PEEKUSER failed: ") + strerror(errno), 14);
    }
    return (uint64_t) val;
}


static void set_hw_breakpoints(pid_t pid, uint64_t addr, int size) {
    ptrace_pokeuser(pid, offsetof(user, u_debugreg[0]), addr);
    ptrace_pokeuser(pid, offsetof(user, u_debugreg[1]), addr);

    int len_encoding = (size == 4) ? 3 : 2;

    uint64_t dr7 = 0;
    dr7 |= 1ULL << 0;
    dr7 |= 1ULL << 2;

    uint64_t field0 = ((uint64_t) 1 << 16) | ((uint64_t) len_encoding << 18);
    uint64_t field1 = ((uint64_t) 3 << 20) | ((uint64_t) len_encoding << 22);

    dr7 |= field0 | field1;
    ptrace_pokeuser(pid, offsetof(user, u_debugreg[7]), dr7);

    ptrace_pokeuser(pid, offsetof(user, u_debugreg[6]), 0);
}

static uint64_t read_debug_status(pid_t pid) {
    return ptrace_peekuser(pid, offsetof(user, u_debugreg[6]));
}

static void clear_debug_status(pid_t pid) {
    ptrace_pokeuser(pid, offsetof(user, u_debugreg[6]), 0);
}

static uint64_t read_variable(pid_t pid, uint64_t addr, int size) {
    uint64_t val = 0;
    uint64_t word = ptrace_peek(pid, addr & ~(sizeof(long) - 1));
    int offset = addr & (sizeof(long) - 1);
    memcpy(&val, reinterpret_cast<char *>(&word) + offset, std::min((size_t) size, sizeof(long) - offset));
    if (size > (int) (sizeof(long) - offset)) {
        uint64_t word2 = ptrace_peek(pid, (addr & ~(sizeof(long) - 1)) + sizeof(long));
        memcpy(reinterpret_cast<char *>(&val) + (sizeof(long) - offset), &word2, size - (sizeof(long) - offset));
    }
    return val;
}

int main(int argc, char **argv) {
    if (argc < 5)
        err_exit("Usage: gwatch --var <symbol> --exec <path> [-- arg1 ... argN]\n", 1);

    std::string varname;
    std::string execpath;
    std::vector<std::string> exec_args;


    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (i == 1 && arg != "--var") {
            err_exit("Usage: gwatch --var <symbol> --exec <path> [-- arg1 ... argN]\n", 1);
        } else if (i == 2) {
            varname = arg;
        } else if (i == 3 && arg != "--exec") {
            err_exit("Usage: gwatch --var <symbol> --exec <path> [-- arg1 ... argN]\n", 1);
        } else if (i == 4) {
            execpath = arg;
        } else if (i > 4) {
            exec_args.emplace_back(arg);
        }
    }


    if (varname.empty() || execpath.empty())
        err_exit("missing --var or --exec", 2);


    auto sym = find_symbol_in_elf(execpath, varname);
    if (!sym)
        err_exit("error: symbol '" + varname + "' not found in " + execpath + "\n", 3);
    if (!sym->is_defined)
        err_exit("error: symbol '" + varname + "' is undefined in " + execpath + "\n", 4);
    if (!(sym->size == 4 || sym->size == 8))
        err_exit(
            "error: unsupported symbol size " + std::to_string(sym->size) + " (must be 4 or 8 bytes as required)\n", 5);


    pid_t child = fork();
    if (child < 0)
        err_exit(std::string("fork failed: ") + strerror(errno), 6);


    if (child == 0) {
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1)
            err_exit(std::string("ptrace TRACEME failed: ") + strerror(errno), 7);


        std::vector<char *> args;
        args.push_back(const_cast<char *>(execpath.c_str()));
        for (auto &s: exec_args)
            args.push_back(const_cast<char *>(s.c_str()));
        args.push_back(nullptr);
        if (execv(execpath.c_str(), args.data()) == -1)
            err_exit(std::string("execv failed: ") + strerror(errno), 8);
    } else {
        int status;
        if (waitpid(child, &status, 0) == -1)
            err_exit(std::string("waitpid failed: ") + strerror(errno), 9);
        if (!WIFSTOPPED(status))
            err_exit("child did not stop after exec", 10);


        auto base_opt = get_base_address_of_mapping(child, execpath);
        if (!base_opt) {
            ptrace(PTRACE_DETACH, child, nullptr, nullptr);
            kill(child, SIGKILL);
            err_exit("error: failed to determine base address via /proc/" + std::to_string(child) + "/maps", 10);
        }

        uint64_t base = *base_opt;
        uint64_t var_addr = base + sym->value;
        int var_size = sym->size;

        uint64_t pr_value = read_variable(child, var_addr, var_size);
        set_hw_breakpoints(child, var_addr, var_size);

        if (ptrace(PTRACE_CONT, child, nullptr, nullptr) == -1)
            err_exit(std::string("ptrace CONT failed: ") + strerror(errno), 11);


        while (true) {
            if (waitpid(child, &status, 0) == -1)
                err_exit(std::string("waitpid failed: ") + strerror(errno), 9);


            if (WIFEXITED(status) || WIFSIGNALED(status))
                break;


            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);
                if (sig == SIGTRAP) {
                    uint64_t dr6 = read_debug_status(child);
                    bool b0 = dr6 & 0x1;
                    bool b1 = dr6 & 0x2;

                    if (!(b0 || b1)) {
                        if (ptrace(PTRACE_CONT, child, nullptr, nullptr) == -1)
                            err_exit(std::string("ptrace CONT failed: ") + strerror(errno), 11);

                        continue;
                    }

                    uint64_t cur_value = read_variable(child, var_addr, var_size);
                    if (b1 && !b0) {
                        std::cout << varname << "\t\t\t\tread\t\t\t" << std::dec << cur_value << "\n";
                    } else {
                        std::cout << varname << "\t\t\t\twrite\t\t\t" << std::dec << pr_value << " -> " << cur_value <<
                                "\n";
                        pr_value = cur_value;
                    }

                    clear_debug_status(child);


                    if (ptrace(PTRACE_CONT, child, nullptr, nullptr) == -1)
                        err_exit(std::string("ptrace CONT failed: ") + strerror(errno), 11);
                } else if (ptrace(PTRACE_CONT, child, nullptr, (void *) (long) sig) == -1)
                    err_exit(std::string("ptrace CONT failed: ") + strerror(errno), 11);
            }
        }


        int exit_code = 0;
        if (WIFEXITED(status))
            exit_code = WEXITSTATUS(status);
        return exit_code == 0 ? 0 : exit_code + 100;
    }

    return 0;
}
