#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <unistd.h>

#include "common/errors.h"
#include "common/io.h"

namespace {
constexpr const char* kProg = "myln";

void print_usage() {
    const char* msg =
        "Usage: myln [-s] [-f] TARGET LINK_NAME\n"
        "Create a link to TARGET with the name LINK_NAME.\n"
        "\n"
        "  -s  create a symbolic link instead of a hard link\n"
        "  -f  remove existing LINK_NAME before creating the link\n"
        "      --help  display this help\n";
    (void)common::write_all(STDOUT_FILENO, msg, std::strlen(msg));
}
}  // namespace

int main(int argc, char* argv[]) {
    bool symbolic = false, force = false;
    std::vector<std::string> operands;

    for (int i = 1; i < argc; ++i) {
        std::string_view a(argv[i]);
        if (a == "--help") {
            print_usage();
            return 0;
        }
        if (a == "--") {
            for (int j = i + 1; j < argc; ++j) operands.emplace_back(argv[j]);
            break;
        }
        // Flag cluster: -s, -f, -sf, -fs (only while no operands seen yet,
        // mirroring mychmod's "flags must come before operands" rule so that
        // a TARGET literally named "-s" still works via "--" or position).
        if (a.size() >= 2 && a[0] == '-' && operands.empty()) {
            bool is_opt = true;
            for (size_t k = 1; k < a.size(); ++k)
                if (a[k] != 's' && a[k] != 'f') {
                    is_opt = false;
                    break;
                }
            if (is_opt) {
                for (size_t k = 1; k < a.size(); ++k)
                    (a[k] == 's') ? symbolic = true : force = true;
                continue;
            }
        }
        operands.emplace_back(a);
    }

    if (operands.size() != 2) {
        const char* m = "Usage: myln [-s] [-f] TARGET LINK_NAME\n";
        (void)common::write_all(STDERR_FILENO, m, std::strlen(m));
        return 1;
    }

    const std::string& target = operands[0];
    const std::string& linkpath = operands[1];

    if (force) {
        // Best-effort: missing dest is fine, nothing to clear.
        (void)::unlink(linkpath.c_str());
    }

    int rc = symbolic ? ::symlink(target.c_str(), linkpath.c_str())
                      : ::link(target.c_str(), linkpath.c_str());
    if (rc != 0) {
        common::print_error(kProg, linkpath);
        return 1;
    }
    return 0;
}
