#!/bin/bash

OS=$(uname -s)

if [[ "$OS" == "Linux" ]]; then
    echo "Scanning for GCC installations..."
    declare -A GCC_MAP

    GCC_LIST=$(compgen -c | grep -E '^gcc(-[0-9]+(\.[0-9]+)*)?$' | sort -u)

    if [ -z "$GCC_LIST" ]; then
        echo "!!! No GCC installations found !!!"
        exit 1
    fi

    for gcc_bin in $GCC_LIST; do
        if command -v "$gcc_bin" &>/dev/null; then
            version=$("$gcc_bin" -dumpfullversion -dumpversion 2>/dev/null)
            if [[ -z "$version" ]]; then
                version=$("$gcc_bin" --version | head -n1 | awk '{print $3}')
            fi
            echo " * Found: $gcc_bin ($version)"
            GCC_MAP["$version"]=$gcc_bin
        fi
    done

    GREATEST_VERSION_FULL=$(printf "%s\n" "${!GCC_MAP[@]}" | sort -V | tail -n1)
    GREATEST_CC="${GCC_MAP[$GREATEST_VERSION_FULL]}"

    IFS='.' read -r major minor patch <<< "$GREATEST_VERSION_FULL"
    GREATEST_VERSION="$major.$minor"

    if command -v "${GREATEST_CC/gcc/g++}" &>/dev/null; then
        GREATEST_CXX="${GREATEST_CC/gcc/g++}"
    else
        GREATEST_CXX="g++"
    fi

    echo
    echo "--> Greatest GCC version detected: $GREATEST_VERSION ($GREATEST_CC/$GREATEST_CXX)"

elif [[ "$OS" == "Darwin" ]]; then
    echo "Detecting macOS compilers..."

    GREATEST_CC=$(command -v clang)
    GREATEST_CXX=$(command -v clang++)

    if [[ -z "$GREATEST_CC" ]] || [[ -z "$GREATEST_CXX" ]]; then
        echo "!!! Clang not found !!!"
        exit 1
    fi

    version_full=$("$GREATEST_CC" --version | head -n1 | awk '{print $4}')

    IFS='.' read -r major minor patch <<< "$version_full"
    GREATEST_VERSION="$major.$minor"

    echo " * Found: $GREATEST_CC ($version_full)"
    echo " * Found: $GREATEST_CXX"
    echo
    echo "--> Using Apple Clang: $GREATEST_VERSION ($GREATEST_CC/$GREATEST_CXX)"

else
    echo "Unsupported OS: $OS"
    exit 1
fi

echo ""
