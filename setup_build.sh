#!/bin/bash

SOURCE_DIR=$(dirname "$(realpath "$0")")

usage() {
    cat <<EOF
Usage: $0 <build_dir> [OPTIONS]

Set up the build environment: detect (or select) a compiler, generate a Conan
profile and a CMake toolchain file, then install dependencies via Conan.

Arguments:
  build_dir                 Output directory for generated files (required)

Options:
  --target <os-arch>        Cross-compile for the given platform instead of
                            building natively. Supported targets:
                              linux-aarch64   ARM64 Linux (RPi3/4/5, Rockchip, ...)
  -h, --help                Show this help message and exit

Examples:
  $0 build                             # native build
  $0 build --target linux-aarch64      # cross-compile for ARM64 Linux
EOF
}

build_dir=""
CROSS_TARGET=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        --target)
            CROSS_TARGET="$2"
            shift 2
            ;;
        *)
            if [ -z "$build_dir" ]; then
                build_dir="$1"
            else
                echo "Unknown argument: $1"
                echo ""
                usage
                exit 1
            fi
            shift
            ;;
    esac
done

if [ -z "$build_dir" ]; then
    usage
    exit 1
fi

MIN_CONAN_VERSION="2.10.0"

if ! command -v conan >/dev/null 2>&1; then
    echo "Conan is not installed. Aborting..."
    exit 1
fi

INSTALLED_CONAN_VERSION=$(conan --version | awk '{print $3}')

if [[ "$(printf '%s\n' "$MIN_CONAN_VERSION" "$INSTALLED_CONAN_VERSION" | sort -V | head -n1)" != "$MIN_CONAN_VERSION" ]]; then
    echo "Conan version $MIN_CONAN_VERSION or higher is required (found $INSTALLED_CONAN_VERSION)"
    exit 1
fi

mkdir -p "$build_dir"

OS=$(uname -s)

TEMPLATE_CMAKE_TOOLCHAIN="$SOURCE_DIR/cmake/toolchain.cmake.template"
OUTPUT_CMAKE_TOOLCHAIN="$build_dir/toolchain.cmake"
TEMPLATE_CONAN_PROFILE="$SOURCE_DIR/conan/profile.txt.template"
OUTPUT_CONAN_PROFILE="$build_dir/profile.txt"

if [ -n "$CROSS_TARGET" ]; then
    echo "Cross-compiling for target: $CROSS_TARGET"

    case "$CROSS_TARGET" in
        linux-aarch64)
            CROSS_CC="aarch64-linux-gnu-gcc"
            CROSS_CXX="aarch64-linux-gnu-g++"
            ARCH_NAME="armv8"
            SYSTEM_PROCESSOR="aarch64"
            OS_NAME="Linux"
            SYSTEM_NAME="Linux"
            COMPILER_NAME="gcc"
            LIBCXX_NAME="libstdc++11"
            ARCH_FLAGS="-march=armv8-a+crc -mtune=generic"
            ;;
        *)
            echo "Unsupported cross-compilation target: $CROSS_TARGET"
            echo "Supported targets: linux-aarch64"
            exit 1
            ;;
    esac

    if ! command -v "$CROSS_CC" >/dev/null 2>&1; then
        echo "Cross-compiler $CROSS_CC not found. Install it with:"
        echo "  sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        exit 1
    fi

    CROSS_VERSION_FULL=$("$CROSS_CC" -dumpfullversion -dumpversion 2>/dev/null)
    IFS='.' read -r major minor patch <<< "$CROSS_VERSION_FULL"
    CROSS_VERSION="$major.$minor"

    echo "Cross-compiler: $CROSS_CC ($CROSS_VERSION_FULL)"

    source "$SOURCE_DIR/tools/setup/detect_compiler.sh"

    OUTPUT_BUILD_PROFILE="$build_dir/profile_build.txt"
    sed \
      -e "s|@OS_NAME@|Linux|g" \
      -e "s|@ARCH_NAME@|x86_64|g" \
      -e "s|@COMPILER_NAME@|gcc|g" \
      -e "s|@LIBCXX_NAME@|libstdc++11|g" \
      -e "s|@MAJOR_VERSION@|${GREATEST_VERSION}|g" \
      -e "s|@BINARY_PATH_CC@|$(command -v $GREATEST_CC)|g" \
      -e "s|@BINARY_PATH_CXX@|$(command -v $GREATEST_CXX)|g" \
      "$TEMPLATE_CONAN_PROFILE" > "$OUTPUT_BUILD_PROFILE"

    sed \
      -e "s|@OS_NAME@|${OS_NAME}|g" \
      -e "s|@ARCH_NAME@|${ARCH_NAME}|g" \
      -e "s|@COMPILER_NAME@|${COMPILER_NAME}|g" \
      -e "s|@LIBCXX_NAME@|${LIBCXX_NAME}|g" \
      -e "s|@MAJOR_VERSION@|${CROSS_VERSION}|g" \
      -e "s|@BINARY_PATH_CC@|$(command -v $CROSS_CC)|g" \
      -e "s|@BINARY_PATH_CXX@|$(command -v $CROSS_CXX)|g" \
      "$TEMPLATE_CONAN_PROFILE" > "$OUTPUT_CONAN_PROFILE"

    sed \
      -e "s|@SYSTEM_NAME@|${SYSTEM_NAME}|g" \
      -e "s|@BINARY_PATH_CC@|$(command -v $CROSS_CC)|g" \
      -e "s|@BINARY_PATH_CXX@|$(command -v $CROSS_CXX)|g" \
      -e "s|@ARCH_FLAGS@|${ARCH_FLAGS}|g" \
      "$TEMPLATE_CMAKE_TOOLCHAIN" > "$OUTPUT_CMAKE_TOOLCHAIN"

    cat >> "$OUTPUT_CMAKE_TOOLCHAIN" <<'CROSS_EOF'
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
CROSS_EOF

    echo ""
    echo "=== Host profile (target: $CROSS_TARGET) ==="
    cat "$OUTPUT_CONAN_PROFILE"
    echo ""
    echo "=== Build profile (native) ==="
    cat "$OUTPUT_BUILD_PROFILE"

    CONAN_PROFILE_ARGS="-pr:h $OUTPUT_CONAN_PROFILE -pr:b $OUTPUT_BUILD_PROFILE"

else
    source "$SOURCE_DIR/tools/setup/detect_compiler.sh"

    if ! conan profile path default > /dev/null 2>&1; then
        conan profile detect > /dev/null 2>&1
    fi
    ARCH_NAME=$(conan profile show -cx host | grep "arch=" | cut -d'=' -f2)

    echo "Architecture for Conan: $ARCH_NAME"

    if [[ "$OS" == "Darwin" ]]; then
        OS_NAME="Macos"
        COMPILER_NAME="apple-clang"
        LIBCXX_NAME="libc++"
        SYSTEM_NAME="Darwin"
        ARCH_NAME="armv8\|x86_64"
    else
        OS_NAME="Linux"
        COMPILER_NAME="gcc"
        LIBCXX_NAME="libstdc++11"
        SYSTEM_NAME="Linux"
    fi

    ARCH_FLAGS=""

    sed \
      -e "s|@SYSTEM_NAME@|${SYSTEM_NAME}|g" \
      -e "s|@BINARY_PATH_CC@|$(command -v $GREATEST_CC)|g" \
      -e "s|@BINARY_PATH_CXX@|$(command -v $GREATEST_CXX)|g" \
      -e "s|@ARCH_FLAGS@|${ARCH_FLAGS}|g" \
      "$TEMPLATE_CMAKE_TOOLCHAIN" > "$OUTPUT_CMAKE_TOOLCHAIN"

    sed \
      -e "s|@OS_NAME@|${OS_NAME}|g" \
      -e "s|@ARCH_NAME@|${ARCH_NAME}|g" \
      -e "s|@COMPILER_NAME@|${COMPILER_NAME}|g" \
      -e "s|@LIBCXX_NAME@|${LIBCXX_NAME}|g" \
      -e "s|@MAJOR_VERSION@|${GREATEST_VERSION}|g" \
      -e "s|@BINARY_PATH_CC@|$(command -v $GREATEST_CC)|g" \
      -e "s|@BINARY_PATH_CXX@|$(command -v $GREATEST_CXX)|g" \
      "$TEMPLATE_CONAN_PROFILE" > "$OUTPUT_CONAN_PROFILE"

    if [[ "$OS" == "Darwin" ]]; then
        echo "OSX_ARCH_VARIANTS=x86_64;arm64" >> "$OUTPUT_CONAN_PROFILE"

        echo "" >> "$OUTPUT_CONAN_PROFILE"
        echo "[platform_tool_requires]" >> "$OUTPUT_CONAN_PROFILE"
    fi
    cat "$OUTPUT_CONAN_PROFILE"

    CONAN_PROFILE_ARGS="-pr $OUTPUT_CONAN_PROFILE -pr:b $OUTPUT_CONAN_PROFILE"
fi

if [[ "${CIBUILDWHEEL:-}" != "1" ]]; then
    conan install "$SOURCE_DIR/conan/conanfile.txt" -of="$build_dir" $CONAN_PROFILE_ARGS --build=missing -s build_type=Debug
fi
conan install "$SOURCE_DIR/conan/conanfile.txt" -of="$build_dir" $CONAN_PROFILE_ARGS --build=missing -s build_type=Release
