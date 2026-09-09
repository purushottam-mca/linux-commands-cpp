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
constexpr const char* kProg = "mychmod";

// Parse "755" -> 0755. Returns false unless 1-4 octal digits <= 07777.
bool parse_octal(std::string_view s, mode_t& out) {
    if (s.empty() || s.size() > 4) return false;
    mode_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '7') return false;
        v = v * 8 + (c - '0');
    }
    if (v > 07777) return false;
    out = v;
    return true;
}

bool apply_one(const std::string& path, mode_t mode, bool verbose, bool& err) {
    if (::chmod(path.c_str(), mode) != 0) {
        common::print_error(kProg, path);
        err = true;
        return false;
    }
    if (verbose) {
        char buf[1024];
        int n = std::snprintf(buf, sizeof(buf), "changed '%s' to %04o\n",
                              path.c_str(), mode);
        if (n > 0) (void)common::write_all(STDOUT_FILENO, buf, n);
    }
    return true;
}

// Bottom-up: children first, directory itself last.
bool apply_recursive(const std::string& path, mode_t mode, bool verbose, bool& err) {
    struct stat st{};
    if (::lstat(path.c_str(), &st) != 0) {
        common::print_error(kProg, path);
        err = true;
        return false;
    }
    if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
        common::UniqueDir dir(::opendir(path.c_str()));
        if (!dir) {
            common::print_error(kProg, path);
            err = true;
            return false;
        }
        struct dirent* ent = nullptr;
        while ((ent = ::readdir(dir.get())) != nullptr) {
            std::string n(ent->d_name);
            if (n == "." || n == "..") continue;
            apply_recursive(path + "/" + n, mode, verbose, err);
        }
    }
    return apply_one(path, mode, verbose, err);
}
}  // namespace

int main(int argc, char* argv[]) {
    bool recursive = false, verbose = false;
    std::string mode_str;
    std::vector<std::string> files;
    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--") {
            for (int j = i + 1; j < argc; ++j) files.emplace_back(argv[j]);
            break;
        }
        if (a.size() >= 2 && a[0] == '-' && mode_str.empty()) {
            bool is_opt = true;
            for (size_t k = 1; k < a.size(); ++k)
                if (a[k] != 'R' && a[k] != 'v') { is_opt = false; break; }
            if (is_opt) {
                for (size_t k = 1; k < a.size(); ++k)
                    (a[k] == 'R') ? recursive = true : verbose = true;
                continue;
            }
        }
        if (mode_str.empty()) mode_str = std::string(a);
        else files.emplace_back(a);
    }
    if (mode_str.empty() || files.empty()) {
        const char* m = "Usage: mychmod [-R] [-v] MODE FILE...\n";
        (void)common::write_all(STDERR_FILENO, m, std::strlen(m));
        return 1;
    }
    mode_t mode = 0;
    if (!parse_octal(mode_str, mode)) {
        std::string e = std::string(kProg) + ": invalid mode '" + mode_str + "' (use octal, e.g. 755)\n";
        (void)common::write_all(STDERR_FILENO, e.c_str(), e.size());
        return 1;
    }
    bool err = false;
    for (auto& f : files) {
        if (recursive) apply_recursive(f, mode, verbose, err);
        else apply_one(f, mode, verbose, err);
    }
    return err ? 1 : 0;
}
