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
[ -d "$ANDROID_HOME" ] || { echo "skip: no ANDROID_HOME" >&2; exit 77; }

JDK="${JAVA_HOME:-$HOME/toolchains/jdk-17.0.20.1+1}"
if [ ! -x "$JDK/bin/java" ]; then
  echo "skip: no JDK at $JDK (export JAVA_HOME)" >&2
  exit 77
fi
export JAVA_HOME="$JDK"
export PATH="$JDK/bin:$PATH"

# The host build of the same JNI shim the APK ships, linked against the same
# host libhunmin.so the rest of the suite uses.
SHIM="$HUNMIN_BUILD_DIR/libhunjni-host.so"
if [ ! -f "$SHIM" ]; then
  "$ROOT/tools/build-host-jni.sh" || exit $?
fi
export HUNMIN_JNI_LIB="$SHIM"

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
