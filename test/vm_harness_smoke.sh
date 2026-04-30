#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp_by="$(mktemp)"
generated_dir="$repo_root/test/vm/bytecode/generated"
bytecode_gen_bin="$repo_root/test/vm/bytecode_gen"
trap 'rm -f "$tmp_by"; rm -rf "$generated_dir"; rm -f "$bytecode_gen_bin"' EXIT

cat >"$tmp_by" <<'EOF'
.code
LOADFALSE R0
TEST      R0, 0
JMP       3
LOADI     R1, 200
RETURN1   R1
JMP       2
LOADI     R1, 100
RETURN1   R1
.end
EOF

jmp_output="$("$repo_root/bin/aqlm" "$tmp_by")"
if [[ "$jmp_output" != "100" ]]; then
    echo "JMP offset smoke failed: expected 100, got '$jmp_output'" >&2
    exit 1
fi

make -C "$repo_root/test/vm" generate-tests >/dev/null

generated_output="$("$repo_root/bin/vmtest" "$repo_root/test/vm/bytecode/generated" "$repo_root/bin/aqlm")"
if ! grep -q "通过: 5" <<<"$generated_output"; then
    echo "generated VM tests did not all pass" >&2
    echo "$generated_output" >&2
    exit 1
fi
