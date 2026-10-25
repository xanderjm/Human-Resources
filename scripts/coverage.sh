#!/usr/bin/env bash
set -euo pipefail

game_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$game_root"
expected="22.1.8"
profdata="${LLVM_PROFDATA:-llvm-profdata}"
cov="${LLVM_COV:-llvm-cov}"
compiler="${LLVM_CC:-clang}"
compiler_cxx="${LLVM_CXX:-clang++}"
for tool in "$profdata" "$cov" "$compiler" "$compiler_cxx"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "$tool from LLVM $expected is required." >&2
        exit 2
    fi
    version="$($tool --version | head -n 1)"
    if [[ "$version" != *"$expected"* ]]; then
        echo "LLVM $expected is required; found: $version" >&2
        exit 2
    fi
done

if [[ "$(uname -s)" == "Darwin" ]]; then
    export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
fi

"$game_root/scripts/build-host-tools.sh"
cmake --preset coverage -S "$game_root" \
    -DCMAKE_C_COMPILER="$compiler" \
    -DCMAKE_CXX_COMPILER="$compiler_cxx"
cmake --build --preset coverage --parallel "${GAME_BUILD_JOBS:-4}"
profile_dir="$game_root/build/coverage/profiles"
cmake -E remove_directory "$profile_dir"
mkdir -p "$profile_dir"
LLVM_PROFILE_FILE="$profile_dir/%p-%m.profraw" ctest --preset coverage --test-dir "$game_root/build/coverage"
"$profdata" merge -sparse "$profile_dir"/*.profraw -o "$game_root/build/coverage/coverage.profdata"
test_binaries=()
while IFS= read -r binary; do
    test_binaries+=("$binary")
done < <(find "$game_root/build/coverage" -maxdepth 1 -type f -name '*_tests' -perm -111 -print | sort)
if [[ "${#test_binaries[@]}" -eq 0 ]]; then
    echo "Coverage build produced no executable contract tests." >&2
    exit 1
fi
coverage_objects=()
for binary in "${test_binaries[@]:1}"; do
    coverage_objects+=("-object=$binary")
done
"$cov" report \
    "${test_binaries[0]}" \
    "${coverage_objects[@]}" \
    -instr-profile="$game_root/build/coverage/coverage.profdata" \
    "$game_root/native"
