#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

tmp_by="$(mktemp)"
trap 'rm -f "$tmp_by"' EXIT

cat >"$tmp_by" <<'BY'
.code
LOADI R0, 42
RETURN1 R0
.end
BY

"$repo_root/bin/aqlvm" "$tmp_by" | grep -qx "42"
trace_output="$("$repo_root/bin/aqlvm" -vt "$tmp_by")"
grep -q "PC=" <<<"$trace_output"
grep -q "LOADI" <<<"$trace_output"
grep -q "RETURN" <<<"$trace_output"
"$repo_root/bin/aqlvm" -vb "$tmp_by" | grep -qx "42"
"$repo_root/bin/aqlvm" -q "$tmp_by" | grep -qx "42"

if "$repo_root/bin/aqlvm" -vl "$tmp_by" >/dev/null 2>&1; then
    echo "aqlvm unexpectedly accepted source-only -vl flag" >&2
    exit 1
fi
