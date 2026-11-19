#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# Upstream's gtest suites (test/), built by mayhem/build.sh with upstream's normal flags. Each binary
# writes a gtest XML report; the counts come from it. A binary that produces no report (crashed,
# exited early) counts as one failure.
TESTS=(
  test/collision/Test_Collision
  test/BulletDynamics/Test_btKinematicCharacterController
  test/InverseDynamics/Test_BulletInverseDynamicsJacobian
  test/InverseDynamics/Test_BulletInverseForwardDynamics
  test/InverseDynamics/Test_BulletInverseDynamics
)
BT="$SRC/mayhem/build-test"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/bullet3-test.XXXXXX")"
passed=0; failed=0; skipped=0
for t in "${TESTS[@]}"; do
  bin="$BT/$t"
  if [ ! -x "$bin" ]; then
    echo "missing test binary $bin (mayhem/build.sh should have built it)" >&2
    failed=$((failed + 1)); continue
  fi
  name="$(basename "$t")"
  xml="$OUT/$name.xml"
  (cd "$(dirname "$bin")" && "./$name" --gtest_output="xml:$xml")
  rc=$?
  if [ ! -s "$xml" ]; then
    echo "$name: no gtest report (exit $rc)" >&2
    failed=$((failed + 1)); continue
  fi
  hdr="$(grep -m1 -o '<testsuites [^>]*>' "$xml")"
  n="$(sed -n 's/.* tests="\([0-9]*\)".*/\1/p' <<<"$hdr")"
  f="$(sed -n 's/.* failures="\([0-9]*\)".*/\1/p' <<<"$hdr")"
  e="$(sed -n 's/.* errors="\([0-9]*\)".*/\1/p' <<<"$hdr")"
  d="$(sed -n 's/.* disabled="\([0-9]*\)".*/\1/p' <<<"$hdr")"
  n=${n:-0}; f=${f:-0}; e=${e:-0}; d=${d:-0}
  if [ "$n" -eq 0 ]; then failed=$((failed + 1)); continue; fi
  bad=$((f + e))
  [ "$rc" -ne 0 ] && [ "$bad" -eq 0 ] && bad=1
  passed=$((passed + n - d - bad)); failed=$((failed + bad)); skipped=$((skipped + d))
  echo "$name: tests=$n failures=$f errors=$e disabled=$d exit=$rc"
done
rm -rf "$OUT"
emit_ctrf "gtest" "$passed" "$failed" "$skipped"
