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

for f in "${ROOT_DIR}"/tests/positive/*.lors; do
  run_positive "${f}"
done

for f in "${ROOT_DIR}"/tests/negative/syntax/*.lors; do
  run_negative "${f}" "syntax"
done

for f in "${ROOT_DIR}"/tests/negative/semantic/*.lors; do
  run_negative "${f}" "semantic"
done

echo
echo "Total passed: ${pass}"
echo "Total failed: ${fail}"

if [[ ${fail} -ne 0 ]]; then
  exit 1
fi

