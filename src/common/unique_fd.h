#pragma once

#include <unistd.h>
#include <utility>

namespace common {

// RAII wrapper for a POSIX file descriptor.
// Non-copyable, movable. Closes fd on destruction via ::close().
// Handles EINTR internally? close() should not be retried on EINTR for Linux
// per man, but we handle gracefully.

class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}

    ~UniqueFd() { reset(); }

    // Non-copyable
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    // Movable
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    int get() const noexcept { return fd_; }
    explicit operator bool() const noexcept { return fd_ != -1; }
    int release() noexcept { return std::exchange(fd_, -1); }

    void reset(int new_fd = -1) noexcept {
        if (fd_ != -1) {
            // POSIX: close() should not be retried after EINTR on Linux.
            // We just call it once; ignore error except EBADF.
            ::close(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_{-1};
};

} // namespace common
