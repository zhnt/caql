#!/usr/bin/env bash

set -euo pipefail

AQLC="${1:-./bin/aqlc}"
AQLVM="${2:-./bin/aqlvm}"
TMP_BY="$(mktemp "${TMPDIR:-/tmp}/aqlc-smoke.XXXXXX.by")"
TMP_LIST="$(mktemp "${TMPDIR:-/tmp}/aqlc-list.XXXXXX.txt")"
TMP_V2="$(mktemp "${TMPDIR:-/tmp}/aqlc-v2.XXXXXX.by")"
TMP_LEGACY="$(mktemp "${TMPDIR:-/tmp}/aqlc-legacy.XXXXXX.by")"
trap 'rm -f "$TMP_BY" "$TMP_LIST" "$TMP_V2" "$TMP_LEGACY"' EXIT

run_chunk_case() {
  local file="$1"
  local expected_file="${file%.aql}.expected"

  "$AQLC" -o "$TMP_BY" "$file"

  actual="$("$AQLVM" "$TMP_BY" 2>&1)"
  expected="$(cat "$expected_file")"
  if [[ "$actual" != "$expected" ]]; then
    echo "aqlc chunk execution mismatch: $file"
    echo "expected:"
    printf '%s\n' "$expected"
    echo "actual:"
    printf '%s\n' "$actual"
    exit 1
  fi
}

run_chunk_case test/regression/arithmetic/basic_add.aql
run_chunk_case test/regression/builtins/string_len.aql
run_chunk_case test/regression/functions/func_closure_return.aql

"$AQLC" -o "$TMP_BY" test/regression/functions/func_closure_return.aql
if ! grep -q '^\.upvalue 0 instack 0 kind 0$' "$TMP_BY"; then
  echo "aqlc chunk missing v2 upvalue structure row"
  cat "$TMP_BY"
  exit 1
fi

cat > "$TMP_V2" <<'CHUNK'
# AQL text chunk v2 without debug upvalue names
.main 0 vararg
.stack 2
K0 STRING "make_reader"
K1 STRING "reader"
.upvalues 1
.upvalue 0 instack 0 kind 0
.code
VARARGPREP 0
CLOSURE R0 0
SETTABUP 0 K0 R0
GETTABUP R0 0 K0
LOADI R1 41
CALL R0 2 2
SETTABUP 0 K1 R0
LOADBUILTIN R0 0 0
GETTABUP R1 0 K1
CALL R1 1 0
CALL R0 0 1
RETURN R0 0 1 0
.end

.function main_child0 1
.stack 2
.code
CLOSURE R1 0
RETURN R1 2 0 1
RETURN R0 0 0 1
.end

.function main_child0_child0 0
.stack 2
.upvalues 1
.upvalue 0 instack 0 kind 0
.code
GETUPVAL R0 0 0
RETURN1 R0
RETURN0
.end
CHUNK

actual="$("$AQLVM" "$TMP_V2" 2>&1)"
if [[ "$actual" != "41" ]]; then
  echo "aqlvm v2 upvalue chunk execution mismatch"
  echo "actual:"
  printf '%s\n' "$actual"
  exit 1
fi

sed -e 's/^\.upvalue 0 instack 0 kind 0$/.upvalue 0 "" instack 0/' \
    -e '/^\.upvalue_name /d' "$TMP_BY" > "$TMP_LEGACY"
actual="$("$AQLVM" "$TMP_LEGACY" 2>&1)"
if [[ "$actual" != "41" ]]; then
  echo "aqlvm legacy upvalue chunk execution mismatch"
  echo "actual:"
  printf '%s\n' "$actual"
  exit 1
fi

"$AQLC" -l test/regression/arithmetic/basic_add.aql > "$TMP_LIST"
if ! grep -q "Function: test/regression/arithmetic/basic_add.aql" "$TMP_LIST"; then
  echo "aqlc disassembly missing function header"
  cat "$TMP_LIST"
  exit 1
fi

if ! grep -q "LOADBUILTIN" "$TMP_LIST"; then
  echo "aqlc disassembly missing bytecode instruction"
  cat "$TMP_LIST"
  exit 1
fi

echo "aqlc chunk smoke passed"
