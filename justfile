yk_config := "/path/to/yk-config"
yk_debug_strs := "true"

build: build-release

# Standard release build: compiled at -O3 with LTO for best interpreter throughput.
build-release:
    mkdir -p cmake-build
    cmake -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_CXX_FLAGS=-I$HOME/.local/include" \
        "-DLIB_CPPUNIT=$HOME/.local/lib/libcppunit.so" \
        -S . -B cmake-build
    cmake --build cmake-build --parallel

# Debug build: no optimisation, assertions enabled, includes unit-test target.
build-debug:
    mkdir -p cmake-debug
    cmake -DCMAKE_BUILD_TYPE=Debug \
        "-DCMAKE_CXX_FLAGS=-I$HOME/.local/include" \
        "-DLIB_CPPUNIT=$HOME/.local/lib/libcppunit.so" \
        -S . -B cmake-debug
    cmake --build cmake-debug --parallel

build-yk-debug:
    mkdir -p cmake-yk-debug
    PATH="$(dirname {{yk_config}}):$PATH" cmake \
        -DCMAKE_CXX_COMPILER=$({{yk_config}} debug --cc)++ \
        -DCMAKE_BUILD_TYPE=Debug \
        -DYK_BUILD_TYPE=debug \
        -DYK_DEBUG_STRS={{yk_debug_strs}} \
        "-DCMAKE_CXX_FLAGS=-I$HOME/.local/include" \
        "-DLIB_CPPUNIT=$HOME/.local/lib/libcppunit.so" \
        -S . -B cmake-yk-debug
    cmake --build cmake-yk-debug --parallel

build-yk-release:
    mkdir -p cmake-yk-release
    PATH="$(dirname {{yk_config}}):$PATH" cmake \
        -DCMAKE_CXX_COMPILER=$({{yk_config}} release --cc)++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DYK_BUILD_TYPE=release \
        -DYK_DEBUG_STRS={{yk_debug_strs}} \
        -S . -B cmake-yk-release
    cmake --build cmake-yk-release --parallel

build-yk: build-yk-release

test: test-unit test-som

test-som: build-release
    cmake-build/SOM++ -cp Smalltalk TestSuite/TestHarness.som

test-unit: build-debug
    cmake-debug/unittests -cp Smalltalk:TestSuite/BasicInterpreterTests Examples/Hello.som

test-yk: build-yk
    cmake-yk-release/SOM++ -cp Smalltalk TestSuite/TestHarness.som

hello: build-release
    cmake-build/SOM++ -cp Smalltalk Examples/Hello.som


hello-yk: build-yk-debug
    cmake-yk/SOM++ -cp Smalltalk Examples/Hello.som

# Lint only files changed relative to HEAD (staged, unstaged, and untracked).
lint-changed:
    #!/usr/bin/env bash
    set -euo pipefail
    # Find a v20 binary; try the versioned name first, then the plain one.
    find_v20() {
        local tool=$1
        for bin in "$tool-20" "$tool"; do
            command -v "$bin" &>/dev/null && "$bin" --version 2>/dev/null | grep -q "version 20" && echo "$bin" && return
        done
        echo "$tool 20 not found. Install with: pip install '$tool==20.*'" >&2
        exit 1
    }
    CLANG_TIDY=$(find_v20 clang-tidy)
    CLANG_FORMAT=$(find_v20 clang-format)
    mapfile -t changed < <(
        { git diff HEAD --name-only --diff-filter=d; git ls-files --others --exclude-standard; } \
        | grep '^src/.*\.\(cpp\|h\)$' | sort -u
    )
    if [[ ${#changed[@]} -eq 0 ]]; then echo "No changed source files."; exit 0; fi
    echo "Linting ${#changed[@]} file(s): ${changed[*]}"
    mapfile -t cpp_files < <(printf '%s\n' "${changed[@]}" | grep '\.cpp$' || true)
    if [[ ${#cpp_files[@]} -gt 0 ]]; then
        for gc in GENERATIONAL MARK_SWEEP COPYING; do
            for integers in "-DUSE_TAGGING=true" "-DUSE_TAGGING=false -DCACHE_INTEGER=true" "-DUSE_TAGGING=false -DCACHE_INTEGER=false"; do
                $CLANG_TIDY --config-file=.clang-tidy "${cpp_files[@]}" -- -fdiagnostics-absolute-paths -DGC_TYPE="$gc" $integers -DUNITTESTS
            done
        done
    fi
    $CLANG_FORMAT --dry-run --style=file --Werror "${changed[@]}"

clean:
    rm -rf cmake-build cmake-debug cmake-yk-debug cmake-yk-release
