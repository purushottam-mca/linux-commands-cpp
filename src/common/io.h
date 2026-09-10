#pragma once

#include <cerrno>
#include <cstddef>
#include <string>

#include <unistd.h>

namespace common {

// Write the entire buffer to fd, retrying on EINTR and looping over partial
// writes. Returns true only if every byte was written; returns false with
// errno set if a non-recoverable error occurs.
//
// Also silences glibc's warn_unused_result on ::write(): the return value is
// assigned and checked, whereas a bare (void) cast does not suppress that
// attribute under -Wunused-result.
inline bool write_all(int fd, const char* buf, std::size_t len) {
    std::size_t off = 0;
    while (off < len) {
        ssize_t n = ::write(fd, buf + off, len - off);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;  // Cannot make progress (e.g. O_NONBLOCK).
        off += static_cast<std::size_t>(n);
    }
    return true;
}

inline bool write_all(int fd, const std::string& s) {
    return write_all(fd, s.data(), s.size());
}

} // namespace common