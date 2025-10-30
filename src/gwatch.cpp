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

using namespace std;

static void err_exit(const string &msg) {
    cerr << msg << "\n";
    exit(1);
}

struct SymbolInfo {
    uint64_t value;
    uint64_t size;
    bool is_defined;
};

static optional<SymbolInfo> find_symbol_in_elf(const string &path, const string &symname) {
    // minimal ELF64 symbol lookup: search .symtab and .dynsym
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return nullopt;

    struct stat st;
    if (fstat(fd, &st) < 0) { close(fd); return nullopt; }
    size_t filesize = st.st_size;

    vector<char> data(filesize);
    ssize_t r = read(fd, data.data(), filesize);
    close(fd);
    if (r != (ssize_t)filesize) return nullopt;

    const unsigned char *mem = reinterpret_cast<const unsigned char*>(data.data());

    if (filesize < sizeof(Elf64_Ehdr)) return nullopt;
    const Elf64_Ehdr *eh = reinterpret_cast<const Elf64_Ehdr*>(mem);
    if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0) return nullopt;
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) return nullopt;

    // section headers
    const Elf64_Shdr *shdrs = reinterpret_cast<const Elf64_Shdr*>(mem + eh->e_shoff);
    const char *shstr = reinterpret_cast<const char*>(mem + shdrs[eh->e_shstrndx].sh_offset);

    auto read_symtab = [&](const Elf64_Shdr &symsh)->optional<SymbolInfo> {
        const char *strtab = nullptr;
        // get linked string table
        if (symsh.sh_link < eh->e_shnum) {
            const Elf64_Shdr &strsh = shdrs[symsh.sh_link];
            strtab = reinterpret_cast<const char*>(mem + strsh.sh_offset);
        }
        const Elf64_Sym *syms = reinterpret_cast<const Elf64_Sym*>(mem + symsh.sh_offset);
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
        return nullopt;
    };

    // search .symtab and .dynsym
    for (int i = 0; i < eh->e_shnum; ++i) {
        const char *secname = shstr + shdrs[i].sh_name;
        if (strcmp(secname, ".symtab") == 0 || strcmp(secname, ".dynsym") == 0) {
            auto res = read_symtab(shdrs[i]);
            if (res)
                return res;
        }
    }
    return nullopt;
}

static optional<uint64_t> get_base_address_of_mapping(pid_t pid, const string &exe_path) {
    // read /proc/<pid>/maps and find first mapping with pathname equal to exe_path
    ostringstream oss;
    oss << "/proc/" << pid << "/maps";
    ifstream f(oss.str());
    if (!f) return nullopt;
    string line;
    int count = 0;
    while (getline(f, line)) {
        std::cout << line << std::endl;
        // format: addr perms offset dev inode pathname
        // we check if line contains exe_path at end
        if (line.find(exe_path) != string::npos) {
            size_t dash = line.find('-');
            if (dash == string::npos) continue;
            string start_hex = line.substr(0, dash);
            uint64_t start = stoull(start_hex, nullptr, 16);
            return start;
        }
    }
    return nullopt;
}

static uint64_t ptrace_peek(pid_t pid, uint64_t addr) {
    errno = 0;
    long word = ptrace(PTRACE_PEEKDATA, pid, (void*)addr, nullptr);
    if (word == -1 && errno) {
        // error
        err_exit(string("ptrace PEEKDATA failed: ") + strerror(errno));
    }
    return (uint64_t)word;
}

static void ptrace_pokeuser(pid_t pid, unsigned reg_offset, uint64_t value) {
    if (ptrace(PTRACE_POKEUSER, pid, (void*)reg_offset, (void*)value) == -1) {
        err_exit(string("ptrace POKEUSER failed: ") + strerror(errno));
    }
}

static uint64_t ptrace_peekuser(pid_t pid, unsigned reg_offset) {
    errno = 0;
    long val = ptrace(PTRACE_PEEKUSER, pid, (void*) reg_offset, nullptr);
    if (val == -1 && errno) {
        err_exit(string("ptrace PEEKUSER failed: ") + strerror(errno));
    }
    return (uint64_t)val;
}

static void set_hw_breakpoints(pid_t pid, uint64_t addr, int size) {
    // We'll use DR0 for write-only and DR1 for read/write (so we can distinguish)
    // DRn registers are at offsets in user area: offsetof(struct user, u_debugreg[n])
    // On x86_64, debug registers are at offsets: offsetof(user, u_debugreg[0]) etc.
    // But ptrace PEEK/POKEUSER uses register offsets in bytes. We use standard offsets:
    // On Linux, debug registers are located in user area starting at the debugreg offset.
    // lib uses offsetof(user, u_debugreg[0]) => 8 * index? We'll compute using sizeof(unsigned long) offset formula:
    // In practice, the offsets are at offsetof(struct user, u_debugreg[0]) == 8 * index?
    // PTRACE_POKEUSER takes an offset in bytes.
    // We use constants from kernel include: debugreg offsets are at 0*8 .. 7*8 for x86_64 in user area.
    // const unsigned debugreg_offset = (sizeof) (struct user);  base for dr0
    //auto user_debug_reg_offset = [&](int n)->unsigned { return  offsetof(struct user, u_debugreg[n]); };

    // DR0 = addr
    ptrace_pokeuser(pid, offsetof(struct user, u_debugreg[0]), addr);
    // DR1 = addr
    ptrace_pokeuser(pid, offsetof(struct user, u_debugreg[1]), addr);

    // Compute encoding for length and access type
    // For x86_64, LEN encoding:
    // 00 - 1 byte, 01 - 2 bytes, 10 - 8 bytes, 11 - 4 bytes
    int len_encoding = -1;
    if (size == 4) len_encoding = 3;
    else if (size == 8) len_encoding = 2;
    else err_exit("unsupported variable size (must be 1,2,4 or 8)");

    // R/W encoding:
    // 00 - exec, 01 - write, 10 - io, 11 - read/write
    int rw_write_only = 1;
    int rw_readwrite = 3;

    // Build DR7
    uint64_t dr7 = 0;
    // enable local for dr0 and dr1: L0 (bit0) and L1 (bit2)
    dr7 |= 1ULL << 0; // L0
    dr7 |= 1ULL << 2; // L1

    // For DR0: set RW0 (bits 16-17) and LEN0 (bits 18-19)
    uint64_t field0 = ((uint64_t)rw_write_only << 16) | ((uint64_t)len_encoding << 18);
    // DR1: bits for RW1 are at 20-21; LEN1 at 22-23
    uint64_t field1 = ((uint64_t)rw_readwrite << 20) | ((uint64_t)len_encoding << 22);

    dr7 |= field0 | field1;

    // poke DR7 (debug control)
    ptrace_pokeuser(pid, offsetof(struct user, u_debugreg[7]), dr7);

    // Clear DR6
    ptrace_pokeuser(pid, offsetof(struct user, u_debugreg[6]), 0);
}

static uint64_t read_debug_status(pid_t pid) {
    return ptrace_peekuser(pid, offsetof(struct user, u_debugreg[6])); // DR6
}

static void clear_debug_status(pid_t pid) {

    ptrace_pokeuser(pid, offsetof(struct user, u_debugreg[6]), 0);
}

static uint64_t read_variable(pid_t pid, uint64_t addr, int size) {
    // read up to 8 bytes with PEEKDATA
    uint64_t val = 0;
    // read word-aligned chunks:
    uint64_t word = ptrace_peek(pid, addr & ~(sizeof(long)-1));
    // adjust offset
    int offset = addr & (sizeof(long)-1);
    memcpy(&val, reinterpret_cast<char*>(&word) + offset, min((size_t)size, sizeof(long)-offset));
    if (size > (int)(sizeof(long)-offset)) {
        // need next word
        uint64_t word2 = ptrace_peek(pid, (addr & ~(sizeof(long)-1)) + sizeof(long));
        memcpy(reinterpret_cast<char*>(&val) + (sizeof(long)-offset), &word2, size - (sizeof(long)-offset));
    }
    return val;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        cerr << "Usage: gwatch --var <symbol> --exec <path> [-- arg1 ... argN]\n";
        return 1;
    }

    string varname;
    string execpath;
    vector<string> exec_args;

    int i = 1;
    while (i < argc) {
        string a = argv[i];
        if (a == "--var") {
            if (++i >= argc) err_exit("missing argument to --var");
            varname = argv[i++];
        } else if (a == "--exec") {
            if (++i >= argc) err_exit("missing argument to --exec");
            execpath = argv[i++];
            // remaining after optional "--" are args
            if (i < argc && string(argv[i]) == "--") {
                ++i;
                for (; i < argc; ++i) exec_args.emplace_back(argv[i]);
            }
        } else {
            // skip / collect as extra
            ++i;
        }
    }

    if (varname.empty() || execpath.empty()) {
        err_exit("missing --var or --exec");
    }

    // Find symbol in ELF (on disk)
    auto sym = find_symbol_in_elf(execpath, varname);
    if (!sym) {
        cerr << "error: symbol '" << varname << "' not found in " << execpath << "\n";
        return 2;
    }
    if (!sym->is_defined) {
        cerr << "error: symbol '" << varname << "' is undefined in " << execpath << "\n";
        return 3;
    }
    if (!(sym->size == 4 || sym->size == 8 || sym->size == 1 || sym->size == 2)) {
        cerr << "error: unsupported symbol size " << sym->size << " (must be 4 or 8 bytes as required)\n";
        return 4;
    }

    pid_t child = fork();
    if (child < 0) {
        err_exit(string("fork failed: ") + strerror(errno));
    }

    if (child == 0) {
        std::cout << "child start" << std::endl;
        // child: trace me and exec
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) == -1) {
            std::cout << "kek\n";
            cerr << "ptrace TRACEME failed: " << strerror(errno) << "\n";
            _exit(1);
        }
        std::cout << "aboba\n";
        // stop ourselves so parent can set breakpoints after exec? We exec, kernel will send SIGTRAP after exec if PTRACE_TRACEME used.
        // Exec target
        vector<char*> args;
        args.push_back(const_cast<char*>(execpath.c_str()));
        for (auto &s : exec_args) args.push_back(const_cast<char*>(s.c_str()));
        args.push_back(nullptr);
        if (execv(execpath.c_str(), args.data()) == -1) {
            std::cout << "lol\n";
            cerr << "execv failed: " << strerror(errno) << "\n";
            _exit(1);
        }
        std::cout << "child end";
    } else {
        // parent
        int status;
        // wait for initial stop (exec or execve will cause SIGTRAP)
        if (waitpid(child, &status, 0) == -1) {
            err_exit(string("waitpid failed: ") + strerror(errno));
        }
        if (!WIFSTOPPED(status)) {
            err_exit("child did not stop after exec");
        }

        // compute runtime address: symbol value + base load
        auto base_opt = get_base_address_of_mapping(child, execpath);
        if (!base_opt) {
            cerr << "error: failed to determine base address via /proc/" << child << "/maps\n";
            // kill child then exit
            ptrace(PTRACE_DETACH, child, nullptr, nullptr);
            kill(child, SIGKILL);
            return 5;
        }
        uint64_t base = *base_opt;
        uint64_t var_addr = base + sym->value;
        int var_size = (int)sym->size;

        // Read initial cached value
        uint64_t cached = read_variable(child, var_addr, var_size);

        // set hardware breakpoints
        set_hw_breakpoints(child, var_addr, var_size);

        // continue child
        if (ptrace(PTRACE_CONT, child, nullptr, nullptr) == -1) {
            err_exit(string("ptrace CONT failed: ") + strerror(errno));
        }

        // main loop
        while (true) {
            if (waitpid(child, &status, 0) == -1) {
                err_exit(string("waitpid failed: ") + strerror(errno));
            }

            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                // child terminated
                int a;
                break;
            }
            if (WIFSTOPPED(status)) {
                int sig = WSTOPSIG(status);
                if (sig == SIGTRAP) {
                    // hardware watchpoint triggered? check DR6
                    uint64_t dr6 = read_debug_status(child);
                    // B0 -> bit0, B1 -> bit1
                    bool b0 = dr6 & 0x1;
                    bool b1 = dr6 & 0x2;

                    // determine access type
                    string access;
                    if (b0) {
                        access = "write"; // write-only breakpoint triggered
                    } else if (b1) {
                        access = "read";
                    } else {
                        // some other trap; pass it along
                        if (ptrace(PTRACE_CONT, child, nullptr, nullptr) == -1) {
                            err_exit(string("ptrace CONT failed: ") + strerror(errno));
                        }
                        continue;
                    }

                    // read current value
                    uint64_t cur = read_variable(child, var_addr, var_size);

                    if (access == "read") {
                        // print: <symbol>    read     <value>
                        cout << varname << "\tread\t" << dec << cur << "\n" << flush;
                        // keep cached unchanged
                    } else { // write
                        // print previous -> current using cache
                        cout << varname << "\twrite\t" << dec << cached << " -> " << cur << "\n" << flush;
                        cached = cur;
                    }

                    // Clear DR6
                   clear_debug_status(child);

                    // let child continue
                    if (ptrace(PTRACE_CONT, child, nullptr, nullptr) == -1) {
                        err_exit(string("ptrace CONT failed: ") + strerror(errno));
                    }
                } else {
                    // forward other signals to child
                    if (ptrace(PTRACE_CONT, child, nullptr, (void*)(long)sig) == -1) {
                        err_exit(string("ptrace CONT failed: ") + strerror(errno));
                    }
                }
            }
        }

        // child exited
        int exit_code = 0;
        if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
        return exit_code;
    }

    return 0;
}
