#pragma once

#include <dirent.h>

namespace common {

class UniqueDir {
public:
    explicit UniqueDir(DIR* d = nullptr) noexcept : dir_(d) {}
    ~UniqueDir() { reset(); }

    UniqueDir(const UniqueDir&) = delete;
    UniqueDir& operator=(const UniqueDir&) = delete;

    UniqueDir(UniqueDir&& other) noexcept : dir_(other.dir_) { other.dir_ = nullptr; }
    UniqueDir& operator=(UniqueDir&& other) noexcept {
        if (this != &other) {
            reset();
            dir_ = other.dir_;
            other.dir_ = nullptr;
        }
        return *this;
    }

    DIR* get() const noexcept { return dir_; }
    explicit operator bool() const noexcept { return dir_ != nullptr; }
    DIR* release() noexcept {
        DIR* tmp = dir_;
        dir_ = nullptr;
        return tmp;
    }
    void reset(DIR* d = nullptr) noexcept {
        if (dir_) ::closedir(dir_);
        dir_ = d;
    }

private:
    DIR* dir_{nullptr};
};

} // namespace common
