#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.h"
#include "unique_dir.h"
#include "write_all.h"

namespace {
constexpr const char* kProg = "myls";

// "-rw-r--r--" style string from st_mode (type + 9 rwx bits).
std::string mode_string(mode_t m) {
    std::string s;
    s += S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : '-';
    s += (m & S_IRUSR) ? 'r' : '-';
    s += (m & S_IWUSR) ? 'w' : '-';
    s += (m & S_IXUSR) ? 'x' : '-';
    s += (m & S_IRGRP) ? 'r' : '-';
    s += (m & S_IWGRP) ? 'w' : '-';
    s += (m & S_IXGRP) ? 'x' : '-';
    s += (m & S_IROTH) ? 'r' : '-';
    s += (m & S_IWOTH) ? 'w' : '-';
    s += (m & S_IXOTH) ? 'x' : '-';
    return s;
}

void emit(const std::string& line) {
    std::string out = line + "\n";
    // Best-effort write; failures (e.g. EPIPE with SIGPIPE default) are ignored.
    (void)common::write_all(STDOUT_FILENO, out.c_str(), out.size());
}

// List one directory. Uses lstat so symlinks are shown, not followed.
bool list_dir(const std::string& path, bool show_all, bool long_fmt) {
    common::UniqueDir dir(::opendir(path.c_str()));
    if (!dir) {
        common::print_error(kProg, path);
        return false;
    }
    std::vector<std::string> names;
    struct dirent* ent = nullptr;
    while ((ent = ::readdir(dir.get())) != nullptr) {
        std::string n(ent->d_name);
        if (!show_all && !n.empty() && n[0] == '.') continue;  // hide dotfiles
        names.push_back(n);
    }
    std::sort(names.begin(), names.end());

    for (auto& n : names) {
        if (!long_fmt) {
            emit(n);
            continue;
        }
        struct stat st{};
        std::string full = path == "/" ? "/" + n : path + "/" + n;
        if (::lstat(full.c_str(), &st) != 0) {
            common::print_error(kProg, full);
            continue;
        }
        std::string line = mode_string(st.st_mode) + " " +
                           std::to_string(st.st_size) + " " + n;
        if (S_ISLNK(st.st_mode)) {
            char buf[1024];
            ssize_t len = ::readlink(full.c_str(), buf, sizeof(buf) - 1);
            if (len >= 0) line += " -> " + std::string(buf, len);
        }
        emit(line);
    }
    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    bool show_all = false, long_fmt = false;
    std::vector<std::string> paths;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--") {
            for (int j = i + 1; j < argc; ++j) paths.emplace_back(argv[j]);
            break;
        }
        if (a.size() >= 2 && a[0] == '-') {
            for (size_t k = 1; k < a.size(); ++k) {
                if (a[k] == 'a') show_all = true;
                else if (a[k] == 'l') long_fmt = true;
                else {
                    std::string e = std::string(kProg) + ": invalid option -- '" + a[k] + "'\n";
                    (void)common::write_all(STDERR_FILENO, e.c_str(), e.size());
                    return 1;
                }
            }
            continue;
        }
        paths.emplace_back(a);
    }
    if (paths.empty()) paths.emplace_back(".");

    bool err = false;
    for (auto& p : paths)
        if (!list_dir(p, show_all, long_fmt)) err = true;
    return err ? 1 : 0;
}
