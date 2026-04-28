#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
SRC_FILE="${ROOT_DIR}/tests/integration/demo_entry.lors"
OBJ_FILE="${BUILD_DIR}/demo_entry.o"
IR_FILE="${BUILD_DIR}/demo_entry.ll"
ASM_FILE="${BUILD_DIR}/demo_entry.s"
BIN_FILE="${BUILD_DIR}/demo_riscv"

"${BUILD_DIR}/lorsc" "${SRC_FILE}" -o "${OBJ_FILE}" --emit-llvm "${IR_FILE}" --emit-asm "${ASM_FILE}" \
  --target riscv64-unknown-linux-gnu

clang --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static \
  "${ROOT_DIR}/runtime/main.c" "${OBJ_FILE}" \
  -o "${BIN_FILE}"

qemu-riscv64 "${BIN_FILE}"
