#!/bin/bash
# Copyright (c) 2026 Jiraiya43
# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Runs a command in the build image, with the project on /work and a copy of the car's system on /sysroot (read-only).
# The copy never goes into git. HU_ROOTFS gives its location; default ~/entrynav2/rootfs.
project="$(cd "$(dirname "$0")/.." && pwd)"
exec docker run --rm -u "$(id -u):$(id -g)" -v "$project:/work" -v "${HU_ROOTFS:-$HOME/entrynav2/rootfs}:/sysroot:ro" -w /work entrynav2-tc:4.7 "$@"
