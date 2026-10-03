#include <cerrno>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/errors.h"
#include "common/io.h"
#include "common/unique_dir.h"

namespace {
constexpr const char* kProg = "myrm";

void print_usage() {
    const char* msg =
        "Usage: myrm [-r] [-f] FILE...\n"
        "Remove files. Directories need -r (removed depth-first).\n"
        "\n"
        "  -r  remove directories and their contents recursively\n"
        "  -f  ignore nonexistent files, never prompt\n"
        "      --help  display this help\n";
    (void)common::write_all(STDOUT_FILENO, msg, std::strlen(msg));
}

// Returns true if path is gone (or was allowed to be missing), false on error.
// Sets err=true on any real failure but always lets the caller continue
// with the remaining paths.
bool remove_recursive(const std::string& path, bool recursive, bool force, bool& err) {
    struct stat st{};
    if (::lstat(path.c_str(), &st) != 0) {
        if (errno == ENOENT && force) return true;  // missing + -f: not an error
        common::print_error(kProg, path);
        err = true;
        return false;
    }

    // lstat never follows symlinks, so a symlink-to-dir lands here as
    // a plain link: one unlink removes just the link, never the target.
    if (!S_ISDIR(st.st_mode)) {
        if (::unlink(path.c_str()) != 0) {
            if (errno == ENOENT && force) return true;
            common::print_error(kProg, path);
            err = true;
            return false;
        }
        return true;
    }

    if (!recursive) {
        common::print_error(kProg, path, EISDIR);
        err = true;
        return false;
    }

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
        remove_recursive(path + "/" + n, recursive, force, err);
    }
    dir.reset();  // closedir before rmdir: tidy, hold nothing across the call
    if (::rmdir(path.c_str()) != 0) {
        if (errno == ENOENT && force) return true;
        common::print_error(kProg, path);
        err = true;
        return false;
    }
    return true;
}
}  // namespace

int main(int argc, char* argv[]) {
    bool recursive = false, force = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--help" && files.empty()) {
            print_usage();
            return 0;
        }
        if (a == "--") {
            for (int j = i + 1; j < argc; ++j) files.emplace_back(argv[j]);
            break;
        }
        // Flag cluster (-r, -f, -rf, -Rf); -R accepted as alias of -r like GNU.
        // Only while no operands seen yet, so a file named "-f" works via "--"
        // or after a first operand -- same rule as myln/mychmod.
        if (a.size() >= 2 && a[0] == '-' && files.empty()) {
            bool is_opt = true;
            for (size_t k = 1; k < a.size(); ++k)
                if (a[k] != 'r' && a[k] != 'R' && a[k] != 'f') {
                    is_opt = false;
                    break;
                }
            if (is_opt) {
                for (size_t k = 1; k < a.size(); ++k)
                    (a[k] == 'f') ? force = true : recursive = true;
                continue;
            }
        }
        files.emplace_back(a);
    }

    if (files.empty()) {
        const char* m = "Usage: myrm [-r] [-f] FILE...\n";
        (void)common::write_all(STDERR_FILENO, m, std::strlen(m));
        return 1;
    }

    bool err = false;
    for (auto& f : files) remove_recursive(f, recursive, force, err);
    return err ? 1 : 0;
}
