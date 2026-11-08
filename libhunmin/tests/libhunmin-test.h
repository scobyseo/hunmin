/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * libhunmin-test.h - shared helpers for libhunmin tier-A C++ tests (#272 P4).
 * Opt-in: include after gtest/test-env and hunmin.h as needed by each test.
 */
#ifndef HUNMIN_LIBHUNMIN_TEST_H_
#define HUNMIN_LIBHUNMIN_TEST_H_

#include <cstdlib>
#include <string>

#include "test-env.h"

extern "C" {
#include "hunmin.h"
}

namespace hunmin_test {

inline std::string env_or(const char *name, const char *fallback) {
    char *buf = hunmin_test_getenv_dup(name);
    std::string out = fallback;
    if (buf) {
        if (*buf) out = buf;
        free(buf);
    }
    return out;
}

inline hunmin_ctx *make_ctx(const char *data_path) {
    const char *dirs[1] = {data_path};
    return hunmin_ctx_new(dirs, 1);
}

inline std::string scratch_dir() {
    std::string out = env_or("TEMP", "");
    if (out.empty()) out = env_or("TMPDIR", "");
    return out.empty() ? std::string(".") : out;
}

} // namespace hunmin_test

#endif
