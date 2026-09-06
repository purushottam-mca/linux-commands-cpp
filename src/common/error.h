#pragma once

#include <cerrno>
#include <cstring>
#include <string>

namespace common {

inline std::string errno_message(const std::string& prefix, int err = errno) {
    return prefix + ": " + std::strerror(err);
}

// Print to stderr in "prog: file: strerror" format matching GNU coreutils.
inline void print_error(const std::string& prog, const std::string& file,
                        int err = errno) {
    std::string msg = prog + ": " + file + ": " + std::strerror(err) + "\n";
    // Use raw write to avoid iostream buffering issues with stderr.
    (void)::write(STDERR_FILENO, msg.c_str(), msg.size());
}

} // namespace common
