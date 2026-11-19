#!/usr/bin/env bash
#
# mayhem/build.sh — build this repo's fuzz harness(es). EDIT per repo.
#
# Runs inside the commit image (mayhem/Dockerfile) as `mayhem` in /mayhem. The base image
# (ghcr.io/savantenvs/base) already exports the build contract — use these, don't redefine:
#   CC, CXX             stock clang / clang++
#   LIB_FUZZING_ENGINE  -fsanitize=fuzzer   (link into each harness that has a LLVMFuzzer entry)
#   SANITIZER_FLAGS     -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
#                       (ASan + UBSan, both set to HALT — so Mayhem catches memory AND UB defects)
#   DEBUG_FLAGS         -g -gdwarf-3   (DWARF debug info — always on for fuzz/standalone builds,
#                       independent of the sanitizer off-switch; DWARF version must be < 4)
#   RUST_DEBUG_FLAGS    -C debuginfo=2 -C force-frame-pointers=yes -C llvm-args=-gdwarf-3
#                       (thread through RUSTFLAGS on every cargo-fuzz build)
#   GO_DEBUG_FLAGS      -gcflags=all=-N -l
#                       (thread through go build / go-fuzz-build so the linked ELF keeps symbols)
#   SRC                 /mayhem (the repo source)
#
# Contract: build EVERYTHING here — one runnable binary per fuzz harness, AND the project's test
# suite (so mayhem/test.sh only has to RUN it, never compile). Keep it ADDITIVE (build upstream as
# upstream documents; don't edit upstream files). IMPORTANT: build the PROJECT ITSELF with
# $SANITIZER_FLAGS and $DEBUG_FLAGS (not just the harness) so the fuzzed code is instrumented
# AND carries DWARF < 4 symbols — otherwise ASan/UBSan only see the harness, not the library
# you're trying to find bugs in, and backtraces won't resolve project source lines. Build the TEST
# suite with the project's NORMAL flags (a clean, independent build) so test.sh stays an honest
# functional oracle and won't false-fail on benign UB. Leave the test binary/runner where test.sh
# expects it.
set -euo pipefail

# clang rejects SOURCE_DATE_EPOCH='' (empty) — it must be unset or a valid integer.
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

# Build knobs come from the ENVIRONMENT (overridable), with sane defaults — no if-statements,
# just parameter-expansion fallbacks. Default sanitizers are the base's ASan+UBSan (halting); override
# per build via the Dockerfile's `--build-arg SANITIZER_FLAGS="..."`.
# NB: SANITIZER_FLAGS uses `=` (no colon) on purpose — `=` only fills when the var is UNSET, so an
# explicit EMPTY value (`--build-arg SANITIZER_FLAGS=`) is honored and builds with NO sanitizers
# (useful when you want the program's natural crash / full backtrace, not an ASan report). The other
# knobs use `:=` (default on empty too). MAYHEM_JOBS sets build parallelism (falls back to nproc).
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
# DEBUG_FLAGS carries DWARF debug info INDEPENDENTLY of the sanitizer off-switch (so an empty
# SANITIZER_FLAGS still yields DWARF symbols). DWARF MUST be < 4 (Mayhem triage can't read >=4); clang-19's
# plain `-g` emits DWARF-5, so `-gdwarf-3` is explicit. Apply $DEBUG_FLAGS to the fuzz/harness/standalone
# builds (NOT the test/oracle build). Rust/Go carry the same intent via their language flags below.
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${RUST_DEBUG_FLAGS:=-C debuginfo=2 -C force-frame-pointers=yes -C llvm-args=-gdwarf-3}"
: "${GO_DEBUG_FLAGS:=-gcflags=all=-N -l}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
# COVERAGE_FLAGS: empty by default → no effect on the normal oracle build. Set it via the Dockerfile's
# `--build-arg COVERAGE_FLAGS="-fprofile-instr-generate -fcoverage-mapping"` to instrument the TEST
# build for source-coverage measurement (how much of the project the test suite actually exercises —
# a quality signal for the oracle; complements the anti-reward-hack sabotage check). APPEND it to the
# test build's compile+link flags in step 3 (NOT the fuzz build); after `test.sh` runs, merge with
# `llvm-profdata` and report with `llvm-cov`. Empty value is honored (`=`, not `:=`).
: "${COVERAGE_FLAGS=}"
export SANITIZER_FLAGS DEBUG_FLAGS RUST_DEBUG_FLAGS GO_DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

# Upstream's CMake build, trimmed to the libraries (no GUI/OpenGL demos, no pybullet, no network
# transports, no VHACD/HACD/obj2sdf). BulletRobotics carries the URDF/SDF/MJCF/OBJ/COLLADA importers
# and the PhysicsDirect server. Build directories live under mayhem/ and are gitignored
# (mayhem/.gitignore), so `git clean -ffdX` removes them.
CMAKE_OPTS=(
  -G Ninja
  -DBUILD_CPU_DEMOS=OFF -DBUILD_BULLET2_DEMOS=OFF -DBUILD_OPENGL3_DEMOS=OFF
  -DBUILD_PYBULLET=OFF -DBUILD_ENET=OFF -DBUILD_CLSOCKET=OFF -DUSE_GLUT=OFF -DBUILD_EGL=OFF
  -DUSE_GRAPHICAL_BENCHMARK=OFF -DBUILD_SHARED_LIBS=OFF -DINSTALL_LIBS=OFF -DENABLE_VHACD=OFF
  -DBUILD_EXTRAS=ON -DBUILD_BULLET_ROBOTICS_GUI_EXTRA=OFF -DBUILD_OBJ2SDF_EXTRA=OFF
  -DBUILD_HACD_EXTRA=OFF -DBUILD_CONVEX_DECOMPOSITION_EXTRA=OFF -DBUILD_GIMPACTUTILS_EXTRA=OFF
)

# SanitizerCoverage on the project too, so libFuzzer/Mayhem see edges inside Bullet, not only in the
# harness. Two UBSan checks are relaxed because Bullet trips them by design on every input:
#   alignment  the .bullet/serializer code reads packed chunk structs at unaligned offsets
#              (bFile.cpp, btSerializer.h) even for upstream's own sample files;
#   null       `&array[0]` on an empty btAlignedObjectArray binds a reference to null (idiomatic in
#              Bullet; an actual null dereference is still caught by ASan as a SEGV).
# ASan and every other UBSan check stay on and halting.
FUZZ_FLAGS="$SANITIZER_FLAGS -fno-sanitize=alignment,null -fsanitize=fuzzer-no-link $DEBUG_FLAGS"

# 1) The project, sanitized, with DWARF 3. Release build type (upstream's default) but -O1 and
#    without -DNDEBUG so assert() stays live.
cmake -S "$SRC" -B "$SRC/mayhem/build-fuzz" "${CMAKE_OPTS[@]}" -DBUILD_UNIT_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS_RELEASE=-O1 -DCMAKE_CXX_FLAGS_RELEASE=-O1 \
  -DCMAKE_C_FLAGS="$FUZZ_FLAGS" -DCMAKE_CXX_FLAGS="$FUZZ_FLAGS"
cmake --build "$SRC/mayhem/build-fuzz" -j"$MAYHEM_JOBS"

# 2) Harnesses: a libFuzzer binary and a run-once -standalone reproducer each.
B="$SRC/mayhem/build-fuzz"
H="$SRC/mayhem/build-harness"
mkdir -p "$H"
INC=(-I"$SRC/src" -I"$SRC/examples" -I"$SRC/examples/ThirdPartyLibs"
     -I"$SRC/Extras/Serialize/BulletFileLoader" -I"$SRC/Extras/Serialize/BulletWorldImporter"
     -I"$SRC/Extras/Serialize/BulletXmlWorldImporter" -I"$SRC/mayhem/harnesses")
LIBS=(-Wl,--start-group
      "$B/Extras/BulletRobotics/libBulletRobotics.a"
      "$B/Extras/Serialize/BulletXmlWorldImporter/libBulletXmlWorldImporter.a"
      "$B/Extras/Serialize/BulletWorldImporter/libBulletWorldImporter.a"
      "$B/Extras/Serialize/BulletFileLoader/libBulletFileLoader.a"
      "$B/Extras/InverseDynamics/libBulletInverseDynamicsUtils.a"
      "$B/src/BulletInverseDynamics/libBulletInverseDynamics.a"
      "$B/src/BulletSoftBody/libBulletSoftBody.a"
      "$B/src/BulletDynamics/libBulletDynamics.a"
      "$B/src/BulletCollision/libBulletCollision.a"
      "$B/src/Bullet3Common/libBullet3Common.a"
      "$B/src/LinearMath/libLinearMath.a"
      -Wl,--end-group -lpthread -ldl)
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$H/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$H/standalone_main.o"
FUZZERS=(bullet_world_importer multibody_world_importer b3_bullet_file urdf_parser sdf_parser
         mjcf_parser xml_world_importer obj_convex_hull stl_loader collada_loader vtk_softbody
         physics_direct_load)
# The Bullet3 reader (src/Bullet3Serialize/Bullet2FileLoader) declares the same bParse:: classes as
# Extras' BulletFileLoader, so its harness links only its own code. It is compiled here, from the
# same sources CMake uses, with two more UBSan checks relaxed on these files only: Thomas Wang's
# integer hash in b3HashMap.h (b3HashInt/b3HashPtr::getHash) shifts and adds on a *signed* int, so
# every pointer key of every .bullet file overflows (LinearMath's btHashMap uses unsigned int).
mkdir -p "$H/b3"
B3OBJS=()
for src in "$SRC"/src/Bullet3Serialize/Bullet2FileLoader/*.cpp "$SRC"/src/Bullet3Common/*.cpp; do
  o="$H/b3/$(basename "$src" .cpp).o"
  $CXX $FUZZ_FLAGS -fno-sanitize=shift-base,signed-integer-overflow -O1 -I"$SRC/src" -c "$src" -o "$o"
  B3OBJS+=("$o")
done
B3LIBS=("${B3OBJS[@]}")
for f in "${FUZZERS[@]}"; do
  if [ "$f" = b3_bullet_file ]; then L=("${B3LIBS[@]}"); else L=("${LIBS[@]}"); fi
  # LoadMeshFromSTL.h is header-only, so it compiles into its harness. Its size check
  # `numTriangles * 50 + 84` overflows a signed int for most triangle counts; that check is relaxed on
  # this one file so the fuzzer can get past it (a wrapped product that matches the file size is how
  # the loader would then read out of bounds, which ASan still reports).
  EXTRA=(); [ "$f" = stl_loader ] && EXTRA=(-fno-sanitize=signed-integer-overflow)
  $CXX $FUZZ_FLAGS "${EXTRA[@]}" -O1 "${INC[@]}" -c "$SRC/mayhem/harnesses/fuzz_$f.cpp" -o "$H/fuzz_$f.o"
  $CXX $FUZZ_FLAGS $LIB_FUZZING_ENGINE "$H/fuzz_$f.o" "$H/lsan_off.o" "${L[@]}" -o "/mayhem/fuzz_$f"
  $CXX $FUZZ_FLAGS "$H/fuzz_$f.o" "$H/standalone_main.o" "$H/lsan_off.o" "${L[@]}" \
    -o "/mayhem/fuzz_$f-standalone"
done

# 3) Upstream's gtest unit tests (test/), a separate build with upstream's normal Release flags.
#    Test_PhysicsClientServer is not built: it links -lBussIK, which only the demo build produces.
cmake -S "$SRC" -B "$SRC/mayhem/build-test" "${CMAKE_OPTS[@]}" -DBUILD_UNIT_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="$COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="$COVERAGE_FLAGS"
cmake --build "$SRC/mayhem/build-test" -j"$MAYHEM_JOBS" --target \
  Test_Collision Test_btKinematicCharacterController Test_BulletInverseDynamicsJacobian \
  Test_BulletInverseForwardDynamics Test_BulletInverseDynamics
