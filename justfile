yk_config := "/path/to/yk-config"
yk_debug_strs := "true"
gc_type := "MARK_SWEEP"
tools_venv := ".venv"

build: build-release

# Standard release build: compiled at -O3 with LTO for best interpreter throughput.
build-release:
    mkdir -p cmake-build
    cmake -DCMAKE_BUILD_TYPE=Release \
        -DGC_TYPE={{gc_type}} \
        "-DCMAKE_CXX_FLAGS=-I$HOME/.local/include" \
        "-DLIB_CPPUNIT=$HOME/.local/lib/libcppunit.so" \
        -S . -B cmake-build
    cmake --build cmake-build --parallel

# Debug build: no optimisation, assertions enabled, includes unit-test target.
build-debug:
    mkdir -p cmake-debug
    cmake -DCMAKE_BUILD_TYPE=Debug \
        -DGC_TYPE={{gc_type}} \
        "-DCMAKE_CXX_FLAGS=-I$HOME/.local/include" \
        "-DLIB_CPPUNIT=$HOME/.local/lib/libcppunit.so" \
        -S . -B cmake-debug
    cmake --build cmake-debug --parallel

build-yk-debug:
    mkdir -p cmake-yk-debug
    PATH="$(dirname {{yk_config}}):$PATH" cmake \
        -DCMAKE_CXX_COMPILER=$({{yk_config}} debug --cxx) \
        -DCMAKE_BUILD_TYPE=Debug \
        -DYK_BUILD_TYPE=debug \
        -DYK_DEBUG_STRS={{yk_debug_strs}} \
        -Dgc_type={{gc_type}} \
        "-DCMAKE_CXX_FLAGS=-I$HOME/.local/include" \
        "-DLIB_CPPUNIT=$HOME/.local/lib/libcppunit.so" \
        -S . -B cmake-yk-debug
    cmake --build cmake-yk-debug --parallel

build-yk-release:
    mkdir -p cmake-yk-release
    PATH="$(dirname {{yk_config}}):$PATH" cmake \
        -DCMAKE_CXX_COMPILER=$({{yk_config}} release --cxx) \
        -DCMAKE_BUILD_TYPE=Release \
        -DYK_BUILD_TYPE=release \
        -DYK_DEBUG_STRS={{yk_debug_strs}} \
        -Dgc_type={{gc_type}} \
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

install-tools:
    python3 -m venv {{tools_venv}}
    {{tools_venv}}/bin/pip install --quiet 'clang-format==20.1.8' 'clang-tidy==20.1.0'

# Format all C++ source under src.
format: install-tools
    find src -type f \( -name '*.cpp' -o -name '*.h' \) -print0 | xargs -0 {{tools_venv}}/bin/clang-format -i --style=file

# Lint all C++ source under src with clang-tidy, across each GC/tagging config.
tidy: install-tools
    #!/usr/bin/env bash
    set -euo pipefail
    for gc in GENERATIONAL MARK_SWEEP COPYING; do
        for integers in "-DUSE_TAGGING=true" "-DUSE_TAGGING=false -DCACHE_INTEGER=true" "-DUSE_TAGGING=false -DCACHE_INTEGER=false -DUSE_VECTOR_PRIMITIVES=false"; do
            {{tools_venv}}/bin/clang-tidy --config-file=.clang-tidy $(find src -name '*.cpp' -not -path 'src/yk/*') -- -fdiagnostics-absolute-paths -isysroot "$(xcrun --show-sdk-path 2>/dev/null || echo /)" -DGC_TYPE="$gc" $integers -DUNITTESTS
        done
    done

clean:
    rm -rf cmake-build cmake-debug cmake-yk-debug cmake-yk-release
