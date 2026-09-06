#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/error.h"
#include "common/unique_fd.h"

// mycat - simplified cat in Modern C++20 + POSIX.
// Supports: cat [OPTION]... [FILE]...
//   -n : number all output lines
//   -E : display $ at end of each line
//   -b : number non-empty lines (GNU extension)
//   -  : stdin
//   No FILE or FILE is - -> stdin.
// Exit 0 on success, 1 if any file failed.

namespace {

    constexpr size_t kBufSize = 64 * 1024;
    constexpr const char* kProg = "mycat";

    struct Options {
        bool number_all = false;      // -n
        bool number_nonblank = false; // -b (implies -n off, GNU: -b overrides -n)
        bool show_ends = false;       // -E
    };

    void print_usage() {
        const char* msg =
            "Usage: mycat [OPTION]... [FILE]...\n"
            "Concatenate FILE(s) to standard output.\n"
            "\n"
            "With no FILE, or when FILE is -, read standard input.\n"
            "\n"
            "  -b  number nonempty output lines, overrides -n\n"
            "  -E  display $ at end of each line\n"
            "  -n  number all output lines\n"
            "      --help  display this help\n";
        (void)::write(STDOUT_FILENO, msg, std::strlen(msg));
    }

    bool cat_file(const std::string& path, Options opts, size_t& line_no, bool& had_error) {
        return true;
    }


};


int main(int argc, char* argv[]) {
    // Ignore SIGPIPE, handle EPIPE via write() return.
    ::signal(SIGPIPE, SIG_IGN);

    Options opts;
    std::vector<std::string> files;
    files.reserve(static_cast<size_t>(argc));

    for (int i = 1; i < argc; ++i) {

        std::string_view arg(argv[i]);
        if (arg == "--help") {
            print_usage();
            return 0;
        }

        if (arg == "--") {
            // Everything after is file even if starts with -
            for (int j = i + 1; j < argc; ++j) 
                files.emplace_back(argv[j]);
            break;
        }

        if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '\0') {
            // Could be -n, -E, -b, -En, -nE, - etc.
            if (arg == "-") {
                files.emplace_back("-");
                continue;
            }

            // Check if it's a negative number? Not for cat, treat as option.
            bool is_option = true;
            for (size_t k = 1; k < arg.size(); ++k) {
                char c = arg[k];
                if (c == 'n') opts.number_all = true;
                else if (c == 'E') opts.show_ends = true;
                else if (c == 'b') opts.number_nonblank = true;
                else if (c == 'A') { // GNU -A = -vET, we simplify to -E
                    opts.show_ends = true;
                } else {
                    // Unknown option
                    std::string err = std::string(kProg) + ": invalid option -- '" + c + "'\n" +
                                      "Try '" + kProg + " --help' for more information.\n";
                    (void)::write(STDERR_FILENO, err.c_str(), err.size());
                    return 1;
                }
            }
            if (is_option) {
                // GNU: -b overrides -n
                if (opts.number_nonblank) opts.number_all = false;
                continue;
            }
        }
        files.emplace_back(argv[i]);
    }

    // GNU cat: -b overrides -n, already handled.
    // If no files, read stdin
    if (files.empty()) {
        files.emplace_back("-");
    }

    size_t line_no = 1;
    bool had_error = false;

    for (auto& f : files) {
        // If write failed due to EPIPE, we should exit immediately (downstream closed).
        // Check if stdout is broken.
        bool ok = cat_file(f, opts, line_no, had_error);
        if (!ok && errno == EPIPE) {
            // Broken pipe from write_all -> exit successfully (GNU behavior)
            return 0;
        }
        // Also check if stdout write failed inside cat_file and set EPIPE
        // We treat had_error false for EPIPE.
    }

    return had_error ? 1 : 0;
}
