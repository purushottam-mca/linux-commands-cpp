#pragma once

#include <cerrno>
#include <cstring>
#include <string>

#include <unistd.h>  // STDERR_FILENO (keep this header self-contained)

#include "write_all.h"

namespace common {

inline std::string errno_message(const std::string& prefix, int err = errno) {
    return prefix + ": " + std::strerror(err);
}

// Print to stderr in "prog: file: strerror" format matching GNU coreutils.
inline void print_error(const std::string& prog, const std::string& file,
                        int err = errno) {
    std::string msg = prog + ": " + file + ": " + std::strerror(err) + "\n";
    // Best-effort raw write to stderr to avoid iostream buffering issues;
    // handles EINTR and partial writes, failures are intentionally ignored.
    (void)write_all(STDERR_FILENO, msg.c_str(), msg.size());
}

} // namespace common
