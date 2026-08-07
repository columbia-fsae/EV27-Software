#!/usr/bin/env bash
# Compiles the SIL harness against the unmodified embedded SOC estimator sources.
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p bin

CC="${CC:-cc}"

"$CC" -O2 -std=c11 -Wall -Wextra \
    -Ic_src \
    c_src/kalman_soc.c \
    c_src/battery_model.c \
    harness/sil_harness.c \
    -o bin/soc_sil \
    -lm

echo "Built bin/soc_sil"
