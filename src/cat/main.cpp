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

#include "common/errors.h"
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
    bool write(int out_fd, const char* data, size_t len) {
        if (::write(out_fd, data, len)) return true;
        if (errno == EPIPE) {
            // Broken pipe (e.g. mycat bigfile | head -n1). Exit silently success.
            // GNU cat exits with 0 in this case when stdout is pipe closed.
            // We return false to signal caller to stop.
            return false;
        }
        common::print_error(kProg, "write error");
        return false;
    }


    // cat to stdout without line processing.
    bool cat_bulk(int input_fd) {
        std::vector<char> buf(kBufSize);
        while(true){
            ssize_t nread = ::read(input_fd, buf.data(), buf.size());
            if(nread < 0){
                if (errno == EINTR) continue;
                return false;
            }
            if(nread == 0) break; // EOF
            if(!write(STDOUT_FILENO, buf.data(), static_cast<size_t>(nread))) {
                // EPIPE -> quiet exit, treat as success for pipeline
                // If write failed due to EPIPE we want to exit 0, not error.
                // Detect via errno
                if (errno == EPIPE) return true;
                return false;
            }
        }
        return true;
    }

    // cat with line-numbering / show-ends.
    // We must buffer input and emit line by line to handle $ and numbering.
    bool cat_with_options(int in_fd, Options opts, size_t& line_no) {
        std::vector<char> buf(kBufSize);
        std::string pending; // leftover without newline from previous read
        pending.reserve(kBufSize);

        auto flush_line = [&](std::string_view line, bool has_newline) -> bool {
            // line does NOT include newline. has_newline indicates original had \n
            std::string out;
            out.reserve(line.size() + 32);

            bool should_number = false;
            if (opts.number_nonblank) {
                should_number = !line.empty();
            } else if (opts.number_all) {
                should_number = true;
            }

            if (should_number) {
                // GNU cat: "%6zu\t" with width 6, right-aligned
                char numbuf[32];
                int len = std::snprintf(numbuf, sizeof(numbuf), "%6zu\t", line_no++);
                out.append(numbuf, static_cast<size_t>(len));
            }

            out.append(line);
            if (opts.show_ends && has_newline) out.push_back('$');
            if (has_newline) out.push_back('\n');

            // For lines without trailing newline (EOF without \n), GNU cat does NOT show $
            return write(STDOUT_FILENO, out.data(), out.size());
        };

        // pending may contain previous partial line. We append new reads and scan for \n.
        // To avoid O(n^2) we use index scanning.
        std::string carry;
        carry.reserve(kBufSize);

        while (true) {
            ssize_t nread = ::read(in_fd, buf.data(), buf.size());
            if (nread < 0) {
                if (errno == EINTR) continue;
                return false;
            }
            if (nread == 0) break;

            size_t start = 0;
            // If we have carry from previous, prepend logically: we maintain carry + new buf scanning
            // Simpler: append to carry, scan, keep leftover.
            carry.append(buf.data(), static_cast<size_t>(nread));

            size_t pos = 0;
            while (true) {
                size_t nl = carry.find('\n', pos);
                if (nl == std::string::npos) break;
                std::string_view line(carry.data() + pos, nl - pos);
                if (!flush_line(line, true)) {
                    if (errno == EPIPE) return true;
                    return false;
                }
                pos = nl + 1;
            }
            // Keep leftover
            if (pos > 0) {
                carry.erase(0, pos);
            }
            // Continue loop, carry holds incomplete last line
            (void)start; // unused
        }

        // EOF: flush remaining if any (line without trailing newline)
        if (!carry.empty()) {
            if (!flush_line(std::string_view(carry.data(), carry.size()), false)) {
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
        } else {
            ok = cat_with_options(raw_fd, opts, line_no);
            if (!ok && errno != EPIPE) {
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
            // Broken pipe from write -> exit successfully (GNU behavior)
            return 0;
        }
        // Also check if stdout write failed inside cat_file and set EPIPE
        // We treat had_error false for EPIPE.
    }

    return had_error ? 1 : 0;
}
