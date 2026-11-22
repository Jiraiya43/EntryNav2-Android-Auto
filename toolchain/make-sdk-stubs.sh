#!/bin/sh
# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Generate link-only ARM libraries; every function traps if a stub is accidentally loaded.
set -eu

manifest=$1
output=$2
mkdir -p "$output"
awk '
    /^#/ || NF == 0 { next }
    NF != 3 || ($1 != "libxsvideo.so.4" && $1 != "libAppFramework.A.so" && $1 != "libCoreFramework.A.so") ||
        $2 != "FUNC" || $3 !~ /^[A-Za-z_][A-Za-z_0-9]*$/ || seen[$1, $3]++ { exit 1 }
' "$manifest"
for soname in libxsvideo.so.4 libAppFramework.A.so libCoreFramework.A.so; do
    assembly="$output/$soname.S"
    versions="$output/$soname.map"
    awk -v lib="$soname" '
        BEGIN { print ".syntax unified\n.arm\n.text" }
        $1 == lib {
            print ".global " $3 "\n.type " $3 ", %function\n" $3 ":\n .word 0xe7f000f0\n.size " $3 ", .-" $3
            count++
        }
        END { if (!count) exit 1 }
    ' "$manifest" > "$assembly"
    awk -v lib="$soname" '
        BEGIN { print "{ global:" }
        $1 == lib { print $3 ";" }
        END { print "local: *; };" }
    ' "$manifest" > "$versions"
    arm-linux-gnueabihf-gcc -march=armv7-a -mfloat-abi=hard -mfpu=neon -shared -nostdlib \
        -Wl,-soname,"$soname" -Wl,--version-script,"$versions" -o "$output/$soname" "$assembly"
    awk -v lib="$soname" '$1 == lib { print $3 }' "$manifest" | sort > "$output/expected"
    arm-linux-gnueabihf-nm -D --defined-only -P "$output/$soname" | awk '{ print $1 }' | sort > "$output/actual"
    diff -u "$output/expected" "$output/actual"
    rm "$assembly" "$versions" "$output/expected" "$output/actual"
done
