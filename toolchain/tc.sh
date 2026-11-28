#!/bin/bash
# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Runs a command in the build image with the car system on /sysroot, or builds isolated SDK outputs with --sdk.
set -euo pipefail
project="$(cd "$(dirname "$0")/.." && pwd)"
if [ "${1:-}" = "--sdk" ]; then
    shift
    if [ "${1:-}" != "make" ]; then
        echo "Usage: bash toolchain/tc.sh --sdk make -j4 all" >&2
        exit 1
    fi
    sdk="$project/artifacts/sdk"
    mkdir -p "$sdk"
    work="$(mktemp -d "$sdk/.work.XXXXXXXX")"
    trap 'rm -rf -- "$work"' EXIT
    tar -C "$project" --exclude='*.o' --exclude='*.d' --exclude='*.so' \
        -cf - Makefile VERSION core platform hmiapp tools | tar -C "$work" -xf -
    docker run --rm -u "$(id -u):$(id -g)" -v "$work:/work" -w /work \
        entrynav2-sdk:4.7 "$@" SYSROOT=/opt/sdk
    for file in entry-aad aa-supervisor hmiapp/libEntryAad.so; do
        if [ ! -f "$work/$file" ]; then
            echo "SDK build did not produce $file" >&2
            exit 1
        fi
    done
    docker run --rm -u "$(id -u):$(id -g)" -v "$work:/work" -w /work \
        entrynav2-sdk:4.7 make -s sdk-link-check SYSROOT=/opt/sdk
    mkdir -p "$sdk/hmiapp"
    install -m 755 "$work/entry-aad" "$work/aa-supervisor" "$sdk/"
    install -m 755 "$work/hmiapp/libEntryAad.so" "$sdk/hmiapp/"
    echo "SDK outputs: $sdk"
    exit 0
fi
# HU_ROOTFS gives the firmware copy's location; default ~/entrynav2/rootfs.
exec docker run --rm -u "$(id -u):$(id -g)" -v "$project:/work" -v "${HU_ROOTFS:-$HOME/entrynav2/rootfs}:/sysroot:ro" -w /work entrynav2-tc:4.7 "$@"
