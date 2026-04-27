#!/bin/bash

VERSION="0.0.0"

if git rev-parse --git-dir > /dev/null 2>&1; then
    TAG=$(git describe --tags --exact-match 2>/dev/null)

    if [[ -n "$TAG" ]]; then
        VERSION="$TAG"
    fi
fi

export VERSION
echo "VERSION set to: $VERSION"
