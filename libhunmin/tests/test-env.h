/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * test-env.h - the portable getenv-duplicate for the tier-A harnesses.
 *
 * The MSVC /W4 build bans getenv (C4996), so the harnesses took _dupenv_s
 * (issue #265) -- which does not exist on Linux, and broke the WSL build
 * (wordset-test.cpp:47, key-rule-test.cpp:819). This is the one wrapper
 * every portable test goes through instead: _dupenv_s where MSVC provides
 * it, getenv + a manual copy elsewhere. The copy is malloc'd; the caller
 * free()s it, exactly as with _dupenv_s. An unset variable gives nullptr;
 * a variable set to the empty string gives a pointer to "" -- the same
 * distinction getenv makes, which the call sites rely on.
 */
#ifndef HUNMIN_TEST_ENV_H_
#define HUNMIN_TEST_ENV_H_

#include <cstdlib>
#include <cstring>

inline char *hunmin_test_getenv_dup(const char *name) {
#if defined(_MSC_VER)
    char *buf = nullptr;
    if (_dupenv_s(&buf, nullptr, name) != 0) return nullptr;
    return buf;
#else
    const char *value = std::getenv(name);
    if (!value) return nullptr;
    const std::size_t n = std::strlen(value) + 1;
    char *buf = static_cast<char *>(std::malloc(n));
    if (buf) std::memcpy(buf, value, n);
    return buf;
#endif
}

#endif /* HUNMIN_TEST_ENV_H_ */
