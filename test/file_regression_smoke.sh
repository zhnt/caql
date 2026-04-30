#!/usr/bin/env bash

set -euo pipefail

BIN_PATH="${1:-./bin/aqld}"

run_case() {
  local file="$1"
  local expected_file="$2"
  local actual
  local expected

  actual="$("$BIN_PATH" "$file" 2>&1)"
  expected="$(cat "$expected_file")"

  if [[ "$actual" != "$expected" ]]; then
    echo "regression mismatch: $file"
    echo "expected:"
    printf '%s\n' "$expected"
    echo "actual:"
    printf '%s\n' "$actual"
    exit 1
  fi
}

run_case "./test/regression/arithmetic/basic_add.aql" "./test/regression/arithmetic/basic_add.expected"
run_case "./test/regression/control_flow/elif_variable_scope.aql" "./test/regression/control_flow/elif_variable_scope.expected"
run_case "./test/regression/control_flow/if_basic.aql" "./test/regression/control_flow/if_basic.expected"
run_case "./test/regression/control_flow/for_numeric.aql" "./test/regression/control_flow/for_numeric.expected"
run_case "./test/regression/control_flow/for_numeric_negative.aql" "./test/regression/control_flow/for_numeric_negative.expected"
run_case "./test/regression/control_flow/for_numeric_sum.aql" "./test/regression/control_flow/for_numeric_sum.expected"
run_case "./test/regression/control_flow/for_range.aql" "./test/regression/control_flow/for_range.expected"
run_case "./test/regression/control_flow/for_range_variables.aql" "./test/regression/control_flow/for_range_variables.expected"

echo "file regression smoke passed"
