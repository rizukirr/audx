#!/bin/bash

BUILD_TYPE="${1,,}"

if [[ "$BUILD_TYPE" == "release" ]]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Relase -DCMAKE_C_COMPILER=clang && cmake --build build --clean-first
else
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang && cmake --build build --clean-first
fi
