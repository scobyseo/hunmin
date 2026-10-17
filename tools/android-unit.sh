#!/usr/bin/env bash
# Tier E1: the Android adapter's JVM unit tests, with the real engine.
#
#   tools/android-unit.sh
#
# Exit 0 pass, 1 fail, 77 skip (no JDK). Registered as ctest `engine-android-e1`.
#
# WHY THIS SCRIPT EXISTS RATHER THAN A PLAIN `gradlew test`:
#
#   Gradle reported BUILD SUCCESSFUL for a run in which all 17 tests were
#   SKIPPED. The tests call assumeTrue("HUNMIN_JNI_LIB unset") because a JVM
#   cannot load an Android .so, and a skip is not a failure -- so an unconfigured
#   checkout produces a green report that measured nothing at all. That is the
#   exact failure shape docs/D3-ROOT-CAUSE.md §"The readiness gate was also wrong"
#   describes, one tier down: the harness went green before it went correct.
#
#   So this script makes the skip LOUD. It requires the host shim, builds it,
#   counts the skipped tests out of the XML, and fails if anything was skipped
#   or if zero tests ran. A missing build step is then a red build, not a green
#   one.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
# Build dir holding the engine .so and host JNI shim. Defaults to build/; a
# packaging build (debian/rules) exports HUNMIN_BUILD_DIR -- issue #8, 2026-10-13.
HUNMIN_BUILD_DIR="${HUNMIN_BUILD_DIR:-$ROOT/build}"
cd "$ROOT/frontends/android"

: "${ANDROID_HOME:=$HOME/Android/Sdk}"
# The default is an MSYS /c/... path, but the consumer is Gradle, a Windows
# JVM: it answers "SDK location not found" for /c/... (measured 2026-10-17,
# issue #86). Same conversion android-trace-emit.sh applies to its own
# default; cygpath -m is identity for C:/ values.
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) export ANDROID_HOME="$(cygpath -m "$ANDROID_HOME")" ;;
esac
[ -d "$ANDROID_HOME" ] || { echo "skip: no ANDROID_HOME" >&2; exit 77; }

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
# Same MSYS-form rule as ANDROID_HOME above, same reason: this value is
# exported as JAVA_HOME further down and Gradle hands it to child processes
# such as its keytool lookup, for which a /c/... form is measured broken
# (docs/ENVIRONMENT.md, the ProcessBuilder("keytool") pitfall).
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) JDK="$(cygpath -m "$JDK")" ;;
esac
if [ ! -x "$JDK/bin/java" ]; then
  echo "skip: no JDK at $JDK (export JAVA_HOME)" >&2
  exit 77
fi
export JAVA_HOME="$JDK"
export PATH="$JDK/bin:$PATH"

# The host build of the same JNI shim the APK ships, linked against the same
# host libhunmin.so the rest of the suite uses.
SHIM="$HUNMIN_BUILD_DIR/libhunjni-host.so"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # build-host-jni.sh produces the DLL name on Windows (it also keeps the
        # lib prefix there); without this branch the check below re-runs the
        # builder and then points HUNMIN_JNI_LIB at a file that was never made,
        # which the tier's assumeTrue turns into a full-suite skip -- the exact
        # "green report that measured nothing" this script exists to prevent.
        SHIM="$(cygpath -m "$SHIM" 2>/dev/null || echo "$SHIM")"
        SHIM="${SHIM%.so}.dll"
        ;;
esac
if [ ! -f "$SHIM" ]; then
  "$ROOT/tools/build-host-jni.sh" || exit $?
fi
export HUNMIN_JNI_LIB="$SHIM"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # The shim's dependent libhunmin.dll must be resolvable from the Gradle
        # test JVM. PATH is the DLL search fallback and the MSYS form is what
        # survives the env conversion (a C:/ entry is re-split at the drive
        # colon, measured 2026-10-17).
        export PATH="$(cygpath -u "$HUNMIN_BUILD_DIR"):$PATH"
        ;;
esac

# --rerun-tasks is load-bearing, not hygiene. Gradle keys the test task on the
# test *classes*, so changing only an environment variable -- which is how this
# tier is configured -- leaves the task UP-TO-DATE and the report on disk is the
# PREVIOUS run's. That produced 17/17 "skipped" from a run that had in fact
# just passed, and cost a round of debugging against stale output. Any green
# report here must come from the run that just happened.
echo "== gradlew :app:testDebugUnitTest (forced)"
./gradlew --quiet :app:testDebugUnitTest --rerun-tasks "$@" || exit 1

# --- the part that matters: did anything actually run? ----------------------
RESULTS="$ROOT/frontends/android/app/build/test-results/testDebugUnitTest"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        # python3 on Windows is a native exe: an MSYS /c/... result dir globs
        # zero files and the run reports "zero tests" while every XML says PASS
        # (measured 2026-10-17). Convert once, here.
        RESULTS="$(cygpath -m "$RESULTS")"
        ;;
esac
[ -d "$RESULTS" ] || { echo "FAIL: no test results at $RESULTS" >&2; exit 1; }

python3 - "$RESULTS" <<'PY'
import glob, sys, xml.etree.ElementTree as ET
res = sys.argv[1]
tot = fail = skip = 0
bad = []
for f in glob.glob(f"{res}/*.xml"):
    r = ET.parse(f).getroot()
    tot += int(r.get("tests", 0))
    fail += int(r.get("failures", 0)) + int(r.get("errors", 0))
    skip += int(r.get("skipped", 0))
    for tc in r.iter("testcase"):
        if tc.find("skipped") is not None:
            bad.append(tc.get("name"))
if tot == 0:
    print("FAIL: zero tests ran -- the suite is not exercising anything", file=sys.stderr)
    sys.exit(1)
if skip:
    print(f"FAIL: {skip}/{tot} tests were SKIPPED, which is a green report that "
          f"measured nothing:", file=sys.stderr)
    for n in bad:
        print(f"    skipped: {n}", file=sys.stderr)
    sys.exit(1)
if fail:
    print(f"FAIL: {fail}/{tot} tests failed", file=sys.stderr)
    sys.exit(1)
print(f"PASS: {tot}/{tot} tier E1 tests ran and passed")
PY
