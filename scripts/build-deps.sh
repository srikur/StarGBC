#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="$ROOT/dependencies/install"
BUILD="$ROOT/dependencies/build"

JOBS="$(getconf _NPROCESSORS_ONLN)"

git -C "$ROOT" submodule update --init --recursive

build_dep() {
    local name="$1"
    shift
    echo "==> Building $name"
    cmake -S "$ROOT/dependencies/$name" -B "$BUILD/$name" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        "$@"
    cmake --build "$BUILD/$name" --parallel "$JOBS"
    cmake --install "$BUILD/$name"
}

build_dep SDL \
    -DSDL_SHARED=OFF \
    -DSDL_STATIC=ON \
    -DSDL_TEST_LIBRARY=OFF \
    -DSDL_TESTS=OFF \
    -DSDL_EXAMPLES=OFF

build_dep spdlog \
    -DSPDLOG_BUILD_SHARED=OFF \
    -DSPDLOG_BUILD_EXAMPLE=OFF \
    -DSPDLOG_BUILD_TESTS=OFF \
    -DSPDLOG_INSTALL=ON

build_dep doctest \
    -DDOCTEST_WITH_TESTS=OFF \
    -DDOCTEST_WITH_MAIN_IN_STATIC_LIB=OFF

echo "==> Done. Static dependencies installed to $PREFIX"
