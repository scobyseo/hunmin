// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Tier D3 — gtest wrapper around the shell-based full-stack runner.
//
// The actual test execution stays in tests/e2e/run-d3.sh: it owns the Xvfb,
// dbus-run-session, fcitx5 startup, harness lifecycle and case loop. This
// file's job is to register the five D3 case groups as gtest tests so they
// fit the same one-executable / one-add_test shape as the engine and TSF
// tiers (issue #118).
//
// Skip contract: run-d3.sh returns 77 when required tooling is absent.
// This binary's main() checks the same fast conditions before gtest runs
// and exits 77 directly, so ctest records a skip rather than a red failure.

#include <gtest/gtest.h>

#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> g_argv;

const std::string& get_arg(const std::string& name) {
    static const std::string kEmpty;
    for (size_t i = 1; i + 1 < g_argv.size(); ++i) {
        if (g_argv[i] == name) return g_argv[i + 1];
    }
    return kEmpty;
}

std::string shell_quote(const std::string& s) {
    std::string result = "'";
    for (char c : s) {
        if (c == '\'') {
            result += "'\\''";
        } else {
            result += c;
        }
    }
    result += "'";
    return result;
}

bool exists_regular(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

bool exists_dir(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool tool_on_path(const std::string& tool) {
    std::string cmd = "command -v " + tool + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

bool python_can_import_gi() {
    return std::system("python3 -c 'import gi' >/dev/null 2>&1") == 0;
}

bool gtk_im_module_present(const std::string& gtk_ver) {
    std::string pattern =
        gtk_ver == "4.0"
            ? "/usr/lib/*/gtk-4.0/*/immodules/im-fcitx5.so"
            : "/usr/lib/*/gtk-3.0/*/immodules/im-fcitx5.so";
    std::string cmd = "ls " + pattern + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

bool check_skip(const std::string& mode) {
    for (const char* tool :
         {"Xvfb", "xwininfo", "xprop", "fcitx5", "python3", "dbus-run-session"}) {
        if (!tool_on_path(tool)) {
            std::cerr << "SKIP: " << tool << " not available" << std::endl;
            return false;
        }
    }
    if (!exists_regular(get_arg("--harness"))) {
        std::cerr << "SKIP: harness not available at "
                  << get_arg("--harness") << std::endl;
        return false;
    }
    if (!exists_regular(get_arg("--cases"))) {
        std::cerr << "SKIP: cases file not found at "
                  << get_arg("--cases") << std::endl;
        return false;
    }
    if (!exists_regular(get_arg("--im-conf")) ||
        !exists_regular(get_arg("--addon-conf"))) {
        std::cerr << "SKIP: fcitx conf files not generated" << std::endl;
        return false;
    }
    if (!exists_dir(get_arg("--addon-dir")) ||
        !exists_dir(get_arg("--sys-pkgdatadir"))) {
        std::cerr << "SKIP: fcitx5 system dirs not found" << std::endl;
        return false;
    }
    if (mode == "gtk3" || mode == "gtk4") {
        std::string ver = get_arg("--gtk-ver").empty() ? "3.0" : get_arg("--gtk-ver");
        if (ver == "4.0" && !tool_on_path("xdotool")) {
            std::cerr << "SKIP: xdotool needed to focus a GTK 4 toplevel"
                      << std::endl;
            return false;
        }
        if (!python_can_import_gi()) {
            std::cerr << "SKIP: no python3 with gobject-introspection (gi)"
                      << std::endl;
            return false;
        }
        if (!gtk_im_module_present(ver)) {
            std::cerr << "SKIP: fcitx5's GTK"
                      << (ver == "4.0" ? "4" : "3")
                      << " input method module not installed" << std::endl;
            return false;
        }
    }
    return true;
}

std::string build_command(const std::string& mode) {
    std::string cmd = "bash " + shell_quote(get_arg("--run-d3")) + " " +
                      shell_quote(get_arg("--sim")) + " " +
                      shell_quote(get_arg("--data")) + " " +
                      shell_quote(get_arg("--harness")) + " " +
                      shell_quote(get_arg("--typer")) + " " +
                      shell_quote(get_arg("--addon-dir")) + " " +
                      shell_quote(get_arg("--im-conf")) + " " +
                      shell_quote(get_arg("--addon-conf")) + " " +
                      shell_quote(get_arg("--src")) + " " +
                      shell_quote(get_arg("--cases")) + " " +
                      shell_quote(get_arg("--sys-addon-dir")) + " " +
                      shell_quote(get_arg("--sys-pkgdatadir"));

    if (mode == "gtk3" || mode == "gtk4") {
        cmd = "HUNMIN_D3_CLIENT=gtk HUNMIN_GTK=" + get_arg("--gtk-ver") + " " + cmd;
    } else if (mode == "candidate") {
        cmd = "HUNMIN_D3_CLIENT=candidate " + cmd;
    }
    if (mode == "edit") {
        cmd = "HUNMIN_D3_CLIENT=xim " + cmd;
        if (!get_arg("--edit-cases").empty()) {
            cmd =
                "HUNMIN_D3_EDIT_CASES=" + get_arg("--edit-cases") + " " + cmd;
        }
    }
    return cmd;
}

} // namespace

TEST(Fcitx5D3, Run) {
    std::string mode = get_arg("--mode");
    std::string cmd = build_command(mode);

    int rc = std::system(cmd.c_str());
    int exit_code = 1;
    if (WIFEXITED(rc)) exit_code = WEXITSTATUS(rc);
    if (exit_code == 77) std::exit(77);
    EXPECT_EQ(exit_code, 0) << "run-d3.sh failed with rc=" << exit_code;
}

int main(int argc, char* argv[]) {
    for (int i = 0; i < argc; ++i) g_argv.emplace_back(argv[i]);

    std::string mode = get_arg("--mode");
    if (mode.empty()) {
        std::cerr << "ERROR: --mode is required" << std::endl;
        return 1;
    }
    if (!check_skip(mode)) return 77;

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
