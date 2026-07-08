#!/usr/bin/env bash

set -eux

CXX=$(yk-config release --cxx)
cmake -S . -B yk-build \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_BUILD_TYPE=Release \
  -DYK_BUILD_TYPE=release
cmake --build yk-build --parallel

yk-build/SOM++ -cp Smalltalk TestSuite/TestHarness.som
