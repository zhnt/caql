#!/usr/bin/env bash

set -euo pipefail

BIN_PATH="${1:-./bin/aqld}"
TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

SCRIPT_FILE="$TMPDIR/expr.aql"
printf '1 + 2\n' > "$SCRIPT_FILE"

file_output="$("$BIN_PATH" "$SCRIPT_FILE" 2>&1)"
if [[ "$file_output" != "3" ]]; then
  echo "file execution mismatch"
  echo "expected: 3"
  echo "actual:   $file_output"
  exit 1
fi

expr_output="$("$BIN_PATH" -e "1 + 2" 2>&1)"
expected_expr_output=$'Evaluating: 1 + 2\nResult: 3'
if [[ "$expr_output" != "$expected_expr_output" ]]; then
  echo "-e execution mismatch"
  echo "expected: $expected_expr_output"
  echo "actual:   $expr_output"
  exit 1
fi

neg_output="$("$BIN_PATH" -e "-42" 2>&1)"
expected_neg_output=$'Evaluating: -42\nResult: -42'
if [[ "$neg_output" != "$expected_neg_output" ]]; then
  echo "unary minus mismatch"
  echo "expected: $expected_neg_output"
  echo "actual:   $neg_output"
  exit 1
fi

bnot_output="$("$BIN_PATH" -e "~15" 2>&1)"
expected_bnot_output=$'Evaluating: ~15\nResult: -16'
if [[ "$bnot_output" != "$expected_bnot_output" ]]; then
  echo "bitwise not mismatch"
  echo "expected: $expected_bnot_output"
  echo "actual:   $bnot_output"
  exit 1
fi

float_output="$("$BIN_PATH" -e "3.14 + 2.86" 2>&1)"
expected_float_output=$'Evaluating: 3.14 + 2.86\nResult: 6'
if [[ "$float_output" != "$expected_float_output" ]]; then
  echo "float expression mismatch"
  echo "expected: $expected_float_output"
  echo "actual:   $float_output"
  exit 1
fi

sub_output="$("$BIN_PATH" -e "100 - 25" 2>&1)"
expected_sub_output=$'Evaluating: 100 - 25\nResult: 75'
if [[ "$sub_output" != "$expected_sub_output" ]]; then
  echo "subtraction mismatch"
  echo "expected: $expected_sub_output"
  echo "actual:   $sub_output"
  exit 1
fi

mod_output="$("$BIN_PATH" -e "17 % 5" 2>&1)"
expected_mod_output=$'Evaluating: 17 % 5\nResult: 2'
if [[ "$mod_output" != "$expected_mod_output" ]]; then
  echo "modulo mismatch"
  echo "expected: $expected_mod_output"
  echo "actual:   $mod_output"
  exit 1
fi

band_output="$("$BIN_PATH" -e "15 & 7" 2>&1)"
expected_band_output=$'Evaluating: 15 & 7\nResult: 7'
if [[ "$band_output" != "$expected_band_output" ]]; then
  echo "bitwise and mismatch"
  echo "expected: $expected_band_output"
  echo "actual:   $band_output"
  exit 1
fi

shr_output="$("$BIN_PATH" -e "20 >> 2" 2>&1)"
expected_shr_output=$'Evaluating: 20 >> 2\nResult: 5'
if [[ "$shr_output" != "$expected_shr_output" ]]; then
  echo "right shift mismatch"
  echo "expected: $expected_shr_output"
  echo "actual:   $shr_output"
  exit 1
fi

bxor_output="$("$BIN_PATH" -e "10 ^ 7" 2>&1)"
expected_bxor_output=$'Evaluating: 10 ^ 7\nResult: 13'
if [[ "$bxor_output" != "$expected_bxor_output" ]]; then
  echo "bitwise xor mismatch"
  echo "expected: $expected_bxor_output"
  echo "actual:   $bxor_output"
  exit 1
fi

echo "cli expression smoke passed"
