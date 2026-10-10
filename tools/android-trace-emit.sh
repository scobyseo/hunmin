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
    else
        echo "skip: no ANDROID_HOME and no $HOME/Android/Sdk" >&2
        exit 77
    fi
fi
[ -d "$ANDROID_HOME" ] || { echo "skip: ANDROID_HOME=$ANDROID_HOME is not a directory" >&2; exit 77; }

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
if [ ! -x "$JDK/bin/java" ]; then
    echo "skip: no JDK at $JDK (export JAVA_HOME)" >&2
    exit 77
fi
export JAVA_HOME="$JDK"
export PATH="$JDK/bin:$PATH"

# Host build of the JNI shim (same as android-unit.sh)
SHIM="$ROOT/build/libhunjni-host.so"
if [ ! -f "$SHIM" ]; then
    "$ROOT/tools/build-host-jni.sh" || exit $?
fi

# --- classpath cache ------------------------------------------------------
# Outside app/build on purpose: `gradlew clean` or a `--rerun-tasks` storm wipes
# that tree, and a cache that lives inside it would be invalidated constantly.
CACHE="$ROOT/build/android-trace-emit-classpath.txt"
LOCK="$ROOT/build/android-trace-emit-classpath.lock"
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
    # up.
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

CP="$(paste -sd: "$CACHE")"

# HUNMIN_JNI_LIB / HUNMIN_REPO_ROOT were passed by the old Gradle task from
# -P properties; TraceEmit reads them from the environment, which is simpler
# and is what runTraceEmit ended up doing anyway (build.gradle.kts:241-242).
HUNMIN_JNI_LIB="$SHIM" HUNMIN_REPO_ROOT="$ROOT" \
    exec "$JDK/bin/java" -cp "$CP" org.hunmin.ime.TraceEmit "$TRACE_IN_ABS"