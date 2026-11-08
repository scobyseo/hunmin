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

#include <gtest/gtest.h>

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

/* One context for the test body, from HUNMIN_DATA_PATH with the repo-relative
 * fallback. A fixture that needs a layer, a staged data tree or its own path
 * overrides SetUp and calls this one first (or creates its own ctx where the
 * path differs); the ctx's lifetime is written once, here. */
class CtxTest : public ::testing::Test {
  protected:
    void SetUp() override {
        ctx_ = make_ctx(env_or("HUNMIN_DATA_PATH", "libhunmin/data").c_str());
        ASSERT_NE(ctx_, nullptr) << "the data tree does not load as a data dir";
    }
    void TearDown() override {
        if (ctx_) hunmin_ctx_free(ctx_);
        ctx_ = nullptr;
    }
    hunmin_ctx *ctx_ = nullptr;
};

} // namespace hunmin_test

#endif
