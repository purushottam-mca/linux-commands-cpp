#pragma once

#include <cerrno>
#include <cstddef>

#include <unistd.h>

namespace common {

// Write all len bytes from buf to fd, retrying on EINTR and short writes so
// output is never silently truncated. Returns true on success; on failure
// returns false and leaves errno set.
inline bool write_all(int fd, const char* buf, size_t len) {
    size_t written = 0;
    while (written < len) {
        ssize_t n = ::write(fd, buf + written, len - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        written += static_cast<size_t>(n);
    }
    return true;
}

} // namespace common