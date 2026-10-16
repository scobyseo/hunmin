#!/usr/bin/env bash
# Tier C Android trace emitter runner.
#
# Reads a .trace.in file, replays it through the Android adapter using the
# host JNI shim, and emits the observed trace to stdout.
#
#   tools/android-trace-emit.sh <trace.in>
#
# Exit codes: 0 success, 1 error, 77 skip (no JDK / no shim / no SDK)
#
# This is the Android counterpart of frontends/fcitx5/build/trace-emit.
# It runs on the HOST (JVM), not on the emulator.
#
# ---------------------------------------------------------------------------
# Why this does not call Gradle per trace
#
# It used to: `./gradlew --quiet :app:runTraceEmit --rerun-tasks …`, once for
# each of the fourteen traces. Two costs, both measured at 2451fdc.
#
# 1. Time. Fourteen Gradle invocations cost 41.7 s of the 185 s serial suite.
#    `--rerun-tasks` re-ran the resource and asset tasks every time, so ~3 s
#    per trace went to work whose result was thrown away.
#
# 2. Correctness under parallelism. Every invocation wrote to the SAME
#    intermediates/ tree under frontends/android/app/build, so the fourteen
#    tests could not run concurrently. `ctest -j16` failed nine of the
#    fourteen with
#      Execution failed for task ':app:processDebugResources'.
#    > A failure occurred while executing …Task$TaskAction
#      > …/app/build/intermediates/local_only_symbol_list/debug/…/R-def.txt
#    i.e. they were deleting each other's files.
#
# So Gradle is now consulted ONCE, to materialise a classpath, and the result
# is cached; the per-trace work is a plain `java` invocation that only reads
# the build tree. Fourteen concurrent `java` processes reading a directory that
# nobody is writing is the whole point.
#
# The cache is guarded by an flock, so concurrent tests that all discover a
# stale cache produce one Gradle run, not fourteen. Freshness is decided by the
# newest mtime under the Android sources and build scripts: a stale classpath
# would silently test old classes, which is the failure mode worth being
# paranoid about here.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# The JVM receiving HUNMIN_REPO_ROOT is a native Windows program: an MSYS
# /c/... value fails its existence check (measured: "data not found at
# \c\Workspace\..."). All program arguments below come from ROOT/CACHE, so
# convert ROOT once, here; POSIX hosts are unaffected.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) ROOT="$(cygpath -m "$ROOT")" ;;
esac
# Build dir holding the engine .so and the host JNI shim. Defaults to build/
# (linux CI job, local `cmake -B build`); a packaging build (debian/rules)
# exports HUNMIN_BUILD_DIR to its own obj-* dir -- issue #8, 2026-10-13.
HUNMIN_BUILD_DIR="${HUNMIN_BUILD_DIR:-$ROOT/build}"
# Convert trace file to absolute path before changing directory
TRACE_IN_ABS="$(realpath "$1")"
cd "$ROOT/frontends/android"

# The SDK probe must NOT default ANDROID_HOME. Writing
#   : "${ANDROID_HOME:=$HOME/Android/Sdk}"
# assigns the default even when the caller unset it, so the existence check
# below passes while Gradle -- which reads the real environment -- still fails
# with "SDK location not found" and this script exits 1 instead of the
# documented 77. Measured: `env -u ANDROID_HOME tools/android-trace-emit.sh …`
# returned rc=1, not rc=77, because of exactly that line.
#
# A default is only supplied when the variable is unset AND the path exists;
# otherwise this is a skip, and 77 is the documented contract (line 9).
if [ -z "${ANDROID_HOME:-}" ]; then
    if [ -d "$HOME/Android/Sdk" ]; then
        # export, not just assign: a plain assignment stays in this shell and
        # Gradle (a child process) still sees no ANDROID_HOME, which is the
        # "SDK location not found" failure measured below.
        export ANDROID_HOME="$HOME/Android/Sdk"
    elif [ -n "${LOCALAPPDATA:-}" ] && [ -d "$LOCALAPPDATA/Android/Sdk" ]; then
        # Android Studio on Windows installs the SDK there.
        export ANDROID_HOME="$(cd "$LOCALAPPDATA/Android/Sdk" && pwd -W 2>/dev/null || pwd)"
    else
        echo "skip: no ANDROID_HOME and no $HOME/Android/Sdk" >&2
        exit 77
    fi
fi
[ -d "$ANDROID_HOME" ] || { echo "skip: ANDROID_HOME=$ANDROID_HOME is not a directory" >&2; exit 77; }

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
JAVA_BIN="$JDK/bin/java"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) JAVA_BIN="$JDK/bin/java.exe" ;;
esac
if [ ! -f "$JAVA_BIN" ]; then
    echo "skip: no JDK at $JDK (export JAVA_HOME)" >&2
    exit 77
fi
export JAVA_HOME="$JDK"
export PATH="$JDK/bin:$PATH"

# Host build of the JNI shim (same as android-unit.sh). HUNMIN_BUILD_DIR
# defaults to build/ and is set by a packaging build (debian/rules) to its own
# dir -- issue #8, measured 2026-10-13.
SHIM="$HUNMIN_BUILD_DIR/libhunjni-host.so"
SHIM_HINT="HUNMIN_BUILD_DIR"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # llvm-mingw keeps the lib/ prefix on Windows shared libraries
        # (measured: build-mingw/ holds libhunmin.dll + libhunmin.dll.a).
        SHIM="$HUNMIN_BUILD_DIR/libhunjni-host.dll"
        ;;
esac
if [ ! -f "$SHIM" ]; then
    # Build-log to stderr, stdout left clean.
    #
    # trace-check.py parses this script's ENTIRE stdout as JSONL (it feeds
    # proc.stdout straight to load_text_as_records), so a single non-JSON line
    # here fails the test. build-host-jni.sh prints its ldd proof and the
    # output path to stdout for a human running it by hand, and on a cold
    # build/ that text landed in front of TraceEmit's output and every one of
    # the fourteen Android conformance tests failed deterministically.
    #
    # The build log is not discarded -- it goes to stderr, which trace-check.py
    # prints only on failure. Fixing it at the call site rather than in
    # build-host-jni.sh keeps that script useful when run directly.
    "$ROOT/tools/build-host-jni.sh" >&2 || exit $?
fi

# --- classpath cache ------------------------------------------------------
# Outside app/build on purpose: `gradlew clean` or a `--rerun-tasks` storm wipes
# that tree, and a cache that lives inside it would be invalidated constantly.
CACHE="$HUNMIN_BUILD_DIR/android-trace-emit-classpath.txt"
LOCK="$HUNMIN_BUILD_DIR/android-trace-emit-classpath.lock"
mkdir -p "$(dirname "$CACHE")"

# Newest mtime over everything whose change can invalidate the classpath.
newest_source_mtime() {
    find app/src app/build.gradle.kts app/proguard-rules.pro \
            settings.gradle.kts build.gradle.kts gradle/wrapper \
         -newer "$CACHE" -print -quit 2>/dev/null || true
}

cache_is_fresh() {
    [ -s "$CACHE" ] || return 1
    [ -z "$(newest_source_mtime)" ]
}

refresh_cache() {
    # flock, not a plain re-check: the check and the write have to be one
    # critical section, or fourteen processes that all found the cache stale
    # each start their own Gradle run and recreate the original race one level
    # up. git-bash ships no flock, so POSIX hosts get fd-based locking and
    # Windows gets a mkdir lock (atomic) with a wait-then-proceed bound: a
    # lock left behind by a killed process must not hang the suite.
    if command -v flock >/dev/null 2>&1; then
        exec {lockfd}>"$LOCK"
        flock "$lockfd"
        if cache_is_fresh; then
            flock -u "$lockfd"
            exec {lockfd}>&-
            return 0
        fi
        ./gradlew --quiet :app:printTraceEmitClasspath >"$CACHE.tmp"
        mv -f "$CACHE.tmp" "$CACHE"
        flock -u "$lockfd"
        exec {lockfd}>&-
    else
        local waited=0
        while ! mkdir "$LOCK" 2>/dev/null; do
            waited=$((waited + 1))
            [ "$waited" -ge 120 ] && break
            sleep 1
        done
        [ "$waited" -ge 120 ] && \
            echo "trace-emit: classpath lock $LOCK looks stale, proceeding unlocked" >&2
        if cache_is_fresh; then
            rmdir "$LOCK" 2>/dev/null || true
            return 0
        fi
        ./gradlew --quiet :app:printTraceEmitClasspath >"$CACHE.tmp"
        mv -f "$CACHE.tmp" "$CACHE"
        rmdir "$LOCK" 2>/dev/null || true
    fi
}

if ! cache_is_fresh; then
    refresh_cache
fi

# --quiet is NOT used here. If the classpath cannot be built the failure must
# reach stderr rather than leaving an empty cache that reads as "up to date"
# and then failing as a confusing NoClassDefFoundError per trace.
if [ ! -s "$CACHE" ]; then
    echo "trace-emit: classpath cache is empty at $CACHE" >&2
    exit 1
fi

# The Windows JVM expects ; between classpath entries (and llvm-mingw's PATH
# for the dependent DLL); POSIX keeps :.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # Gradle (a Windows JVM) ends each line CRLF, and a CR that survives
        # into a -cp entry is part of the path for the loader -- measured as
        # ClassNotFoundException: org.hunmin.ime.TraceEmit. Strip it here;
        # POSIX keeps the plain paste.
        CP="$(tr -d '\r' < "$CACHE" | paste -sd';')"
        # The JVM loader must resolve the shim's dependent engine DLL; putting
        # the build dir on PATH does that for a native child of this bash. The
        # entry must be the MSYS form: a C:/... entry in PATH is re-split at the
        # drive colon when the list is converted for the child, and the engine
        # then resolves from whatever other PATH dir happens to carry a stale
        # libhunmin.dll (measured 2026-10-17).
        export PATH="$(cygpath -u "$HUNMIN_BUILD_DIR"):$PATH"
        ;;
    *)
        CP="$(paste -sd: "$CACHE")"
        ;;
esac

# HUNMIN_JNI_LIB / HUNMIN_REPO_ROOT were passed by the old Gradle task from
# -P properties; TraceEmit reads them from the environment, which is simpler
# and is what runTraceEmit ended up doing anyway (build.gradle.kts:241-242).
#
# The stdout encoding is pinned because the JVM does not inherit a UTF-8 locale
# the way CPython does. Under debian/rules' `export LC_ALL=C` (measured in the
# obj-x86_64-linux-gnu build tree, all 14 Android conformance cases) Java 17's
# default stdout charset is US-ASCII and every Korean preedit became '?': the
# JSON still parsed -- that half of the cold-build failure was build-host-jni.sh
# leaking its ldd proof into stdout -- but the values were destroyed. Python
# coerces the C locale to UTF-8 (PEP 540) and is why the x11 half passed in the
# same run. Both properties: file.encoding covers older code paths,
# stdout.encoding covers the PrintStream itself (18+ ignore sun.*, 17 knows only
# sun.stdout.encoding -- set the pair that each version reads).
HUNMIN_JNI_LIB="$SHIM" HUNMIN_REPO_ROOT="$ROOT" \
    exec "$JDK/bin/java" -Dfile.encoding=UTF-8 \
    -Dsun.stdout.encoding=UTF-8 -Dstdout.encoding=UTF-8 \
    -cp "$CP" org.hunmin.ime.TraceEmit "$TRACE_IN_ABS"