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

    // Write all bytes handling partial writes and EINTR.
    // Returns true on success, false on EPIPE (broken pipe) -> should exit quietly,
    // or other error -> reports to stderr.
    // For SIGPIPE we ignore signal and handle EPIPE ourselves to match GNU cat behaviour.
    bool write_all(int out_fd, const char* data, size_t len) {
        size_t written = 0;
        while (written < len) {
            ssize_t n = ::write(out_fd, data + written, len - written);
            if (n < 0) {
                if (errno == EINTR) continue;
                if (errno == EPIPE) {
                    // Broken pipe (e.g. mycat bigfile | head -n1). Exit silently success.
                    // GNU cat exits with 0 in this case when stdout is pipe closed.
                    // We return false to signal caller to stop.
                    return false;
                }
                common::print_error(kProg, "write error");
                return false;
            }
            written += static_cast<size_t>(n);
        }
        return true;
    }


    // bulk copy fd -> stdout without line processing.
    bool cat_bulk(int input_fd) {
        std::vector<char> buf(kBufSize);
        while(true){
            ssize_t nread = ::read(input_fd, buf.data(), buf.size());
            if(nread < 0){
                if (errno == EINTR) continue;
                return false;
            }
            if(nread == 0) break; // EOF
            if(!write_all(STDOUT_FILENO, buf.data(), static_cast<size_t>(nread))) {
                // EPIPE -> quiet exit, treat as success for pipeline
                // If write_all failed due to EPIPE we want to exit 0, not error.
                // Detect via errno
                if (errno == EPIPE) return true;
                return false;
            }
        }
        return true;
    }

    bool cat_file(const std::string& path, Options opts, size_t& line_no, bool& had_error) {
        int raw_fd = -1;
        common::UniqueFd owned_fd;
        bool is_stdin = (path == "-");

        if(is_stdin){
            raw_fd = STDIN_FILENO;
        } else {
            int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if(fd < 0){
                common::print_error(kProg, path);
                had_error = true;
                return false;
            }
            owned_fd.reset(fd);

            // if it's a directory then error like cat: "Is a directory"
            struct stat st{};
            if (::fstat(owned_fd.get(), &st) == 0 && S_ISDIR(st.st_mode)) {
                common::print_error(kProg, path, EISDIR);
                had_error = true;
                return false;
            }
            raw_fd = owned_fd.get();
        }

        bool ok = true;
        // fast (when no option is passed with mycat)
        if(!opts.number_all && !opts.number_nonblank && !opts.show_ends) {
            ok = cat_bulk(raw_fd);
            if(!ok && errno != EPIPE){
                int saved = errno;
                common::print_error(kProg, path, saved);
                had_error = true;
            } else if (!ok && errno == EPIPE) {
                ok = true;
            }
        } 
        return ok;
    }

}


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
