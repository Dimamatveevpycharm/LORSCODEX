#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <path-to-lorsc-binary>"
  exit 1
fi

LORSC="$1"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="${ROOT_DIR}/build/test-artifacts"
mkdir -p "${TMP_DIR}"
CLANG_BIN="${CLANG:-clang}"
QEMU_BIN="${QEMU_RISCV64:-qemu-riscv64}"

pass=0
fail=0

run_positive() {
  local f="$1"
  local name
  name="$(basename "${f}" .lors)"
  if "${LORSC}" "${f}" -o "${TMP_DIR}/${name}.o" --emit-llvm "${TMP_DIR}/${name}.ll" >/dev/null 2>"${TMP_DIR}/${name}.err"; then
    ((pass+=1))
    echo "[PASS] positive ${name}"
  else
    ((fail+=1))
    echo "[FAIL] positive ${name}"
    cat "${TMP_DIR}/${name}.err"
  fi
}

run_negative() {
  local f="$1"
  local group="$2"
  local name
  name="$(basename "${f}" .lors)"
  if "${LORSC}" "${f}" -o "${TMP_DIR}/${name}.o" >/dev/null 2>"${TMP_DIR}/${name}.err"; then
    ((fail+=1))
    echo "[FAIL] ${group} ${name}: expected failure, got success"
  else
    if grep -q "error:" "${TMP_DIR}/${name}.err"; then
      ((pass+=1))
      echo "[PASS] ${group} ${name}"
    else
      ((fail+=1))
      echo "[FAIL] ${group} ${name}: no diagnostic"
      cat "${TMP_DIR}/${name}.err"
    fi
  fi
}

run_integration() {
  local f="$1"
  local name
  local expected_file
  local actual_output
  local expected_output

  name="$(basename "${f}" .lors)"
  expected_file="${f%.lors}.expected"

  if [[ ! -f "${expected_file}" ]]; then
    ((fail+=1))
    echo "[FAIL] integration ${name}: missing ${expected_file}"
    return
  fi

  if ! "${LORSC}" "${f}" -o "${TMP_DIR}/${name}.o" --emit-llvm "${TMP_DIR}/${name}.ll" \
    --emit-asm "${TMP_DIR}/${name}.s" >/dev/null 2>"${TMP_DIR}/${name}.err"; then
    ((fail+=1))
    echo "[FAIL] integration ${name}: compilation failed"
    cat "${TMP_DIR}/${name}.err"
    return
  fi

  if [[ ! -s "${TMP_DIR}/${name}.s" ]]; then
    ((fail+=1))
    echo "[FAIL] integration ${name}: assembly file was not generated"
    return
  fi

  if ! "${CLANG_BIN}" --target=riscv64-unknown-linux-gnu -fuse-ld=lld -static \
    "${ROOT_DIR}/runtime/main.c" "${TMP_DIR}/${name}.o" -o "${TMP_DIR}/${name}.riscv" \
    >/dev/null 2>"${TMP_DIR}/${name}.link.err"; then
    ((fail+=1))
    echo "[FAIL] integration ${name}: link failed"
    cat "${TMP_DIR}/${name}.link.err"
    return
  fi

  if ! actual_output="$("${QEMU_BIN}" "${TMP_DIR}/${name}.riscv" 2>"${TMP_DIR}/${name}.run.err")"; then
    ((fail+=1))
    echo "[FAIL] integration ${name}: qemu run failed"
    cat "${TMP_DIR}/${name}.run.err"
    return
  fi

  expected_output="$(<"${expected_file}")"
  if [[ "${actual_output}" == "${expected_output}" ]]; then
    ((pass+=1))
    echo "[PASS] integration ${name}"
  else
    ((fail+=1))
    echo "[FAIL] integration ${name}: unexpected output"
    echo "Expected: ${expected_output}"
    echo "Actual:   ${actual_output}"
  fi
}

for f in "${ROOT_DIR}"/tests/positive/*.lors; do
  run_positive "${f}"
done

for f in "${ROOT_DIR}"/tests/negative/syntax/*.lors; do
  run_negative "${f}" "syntax"
done

for f in "${ROOT_DIR}"/tests/negative/semantic/*.lors; do
  run_negative "${f}" "semantic"
done

if command -v "${CLANG_BIN}" >/dev/null 2>&1 && command -v "${QEMU_BIN}" >/dev/null 2>&1; then
  for f in "${ROOT_DIR}"/tests/integration/*.lors; do
    run_integration "${f}"
  done
else
  echo "[SKIP] integration tests require ${CLANG_BIN} and ${QEMU_BIN}"
fi

echo
echo "Total passed: ${pass}"
echo "Total failed: ${fail}"

if [[ ${fail} -ne 0 ]]; then
  exit 1
fi
