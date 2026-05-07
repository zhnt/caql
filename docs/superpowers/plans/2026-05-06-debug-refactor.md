# Debug Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current overlapping debug systems with one public debug facade, VM-specific trace hooks, and isolated internal diagnostics while preserving current CLI behavior.

**Architecture:** `src/adebug.h/.c` becomes the only public debug facade for flags, sinks, formatting policy, and CLI option mapping. `src/avm_debug.c` owns VM instruction trace formatting and is reached through release-safe hook macros. `src/adebug_internal.h/.c` remains for developer-only diagnostics and is explicitly separated from user-facing `-v/-vt/-vb/-vl/-vast` output.

**Tech Stack:** C11, existing Makefile targets, existing `aql`, `aqld`, `aqlvm`, `vmtest`, Lua 5.5 VM alignment tests.

---

## File Structure

- Modify: `src/adebug.h`
  Public debug API, canonical flag enum, release/debug no-op macros, CLI flag parser declarations, output sink declarations.

- Modify: `src/adebug.c`
  Canonical debug state, flag enable/disable logic, option parsing, output formatting helpers, value formatting shared by bytecode and VM trace.

- Modify: `src/adebug_user.h`
  Temporary compatibility header. Initially map old `AQL_DEBUG_*` names and `aqlD_*` calls to the canonical `adebug` API. Later shrink or remove.

- Modify: `src/adebug_user.c`
  Temporary compatibility implementation. Initially forward existing `aqlD_*` functions to `adebug.c`; later migrate unique pretty-printer functions into `adebug.c` or a focused formatter file.

- Modify: `src/avm.h`
  Keep only VM debug hook macros and declarations. Release builds must compile every VM debug hook to `((void)0)`.

- Modify: `src/avm_debug.c`
  Own all VM trace formatting. No global `current_*` context after refactor; pass VM context explicitly per hook call.

- Modify: `src/avm_core.c`
  Replace direct debug macro calls and ad hoc debug printing with narrow hook calls.

- Modify: `src/main.c`
  Use shared debug CLI parser. Keep `aql` support for source-level flags: `-vl`, `-vast`, `-vb`, `-vt`, `-vd`, `-v`, `-st`, `-sa`, `-sb`.

- Modify: `src/aqlvm.c`
  Use shared debug CLI parser with the VM tool flag subset: `-v`, `-vt`, `-vd`, `-vb`, `-q`.

- Modify: `docs/debug-design.md`, `docs/debug-req.md`, `AGENTS.md`
  Update naming, layering, and recommended validation commands.

- Add: `test/debug/` if it does not exist.
  Small shell-level tests for CLI flag behavior and stable output expectations.

---

### Task 1: Add Canonical Debug Flags And Compatibility Mapping

**Files:**
- Modify: `src/adebug.h`
- Modify: `src/adebug.c`
- Modify: `src/adebug_user.h`
- Modify: `src/adebug_user.c`
- Test: `test/debug/debug_flags_smoke.sh`

- [ ] **Step 1: Write the failing smoke test**

Create `test/debug/debug_flags_smoke.sh`:

```bash
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
"$repo_root/bin/aqlvm" -vt "$tmp_by" | grep -q "PC="
"$repo_root/bin/aqlvm" -vb "$tmp_by" | grep -q "字节码"
"$repo_root/bin/aqlvm" -q "$tmp_by" | grep -qx "42"
```

- [ ] **Step 2: Run test to verify current baseline**

Run: `bash test/debug/debug_flags_smoke.sh`

Expected: PASS before refactor. If it fails, capture the current behavior and adjust only if the current CLI contract is different from the documented one.

- [ ] **Step 3: Introduce canonical flags**

In `src/adebug.h`, add one canonical public enum:

```c
typedef unsigned int AQLDebugMask;

typedef enum AQLDebugFlag {
  AQL_DBG_NONE    = 0u,
  AQL_DBG_DETAIL  = 1u << 0,
  AQL_DBG_VMTRACE = 1u << 1,
  AQL_DBG_AST     = 1u << 2,
  AQL_DBG_LEX     = 1u << 3,
  AQL_DBG_CODE    = 1u << 4,
  AQL_DBG_ALL     = AQL_DBG_DETAIL | AQL_DBG_VMTRACE | AQL_DBG_AST |
                    AQL_DBG_LEX | AQL_DBG_CODE
} AQLDebugFlag;
```

Keep old names as compatibility aliases in the same header for this task:

```c
#define AQL_FLAG_VD   AQL_DBG_DETAIL
#define AQL_FLAG_VT   AQL_DBG_VMTRACE
#define AQL_FLAG_VAST AQL_DBG_AST
#define AQL_FLAG_VL   AQL_DBG_LEX
#define AQL_FLAG_VB   AQL_DBG_CODE
#define AQL_FLAG_V    AQL_DBG_ALL
```

In `src/adebug_user.h`, map old `AQL_DEBUG_*` names to the same canonical flags:

```c
#define AQL_DEBUG_NONE  AQL_DBG_NONE
#define AQL_DEBUG_LEX   AQL_DBG_LEX
#define AQL_DEBUG_PARSE AQL_DBG_AST
#define AQL_DEBUG_CODE  AQL_DBG_CODE
#define AQL_DEBUG_VM    AQL_DBG_VMTRACE
#define AQL_DEBUG_ALL   AQL_DBG_ALL
```

- [ ] **Step 4: Unify runtime state**

In `src/adebug.c`, make `aql_debug_set_flags`, `aql_debug_enable_flag`, `aql_debug_disable_all`, `aql_debug_is_enabled`, `aqlD_init_debug`, and `aqlD_set_debug_flags` operate on the same internal mask.

Do not keep two independent global masks.

- [ ] **Step 5: Verify**

Run:

```bash
make aqlvm
bash test/debug/debug_flags_smoke.sh
make both
git diff --check
```

Expected:
- `debug_flags_smoke.sh` passes.
- `make both` passes with only known pre-existing warnings.
- No whitespace errors.

- [ ] **Step 6: Commit**

```bash
git add src/adebug.h src/adebug.c src/adebug_user.h src/adebug_user.c test/debug/debug_flags_smoke.sh
git commit -m "refactor(debug): unify debug flag state"
```

---

### Task 2: Centralize Debug CLI Option Parsing

**Files:**
- Modify: `src/adebug.h`
- Modify: `src/adebug.c`
- Modify: `src/main.c`
- Modify: `src/aqlvm.c`
- Test: `test/debug/debug_flags_smoke.sh`

- [ ] **Step 1: Add parser API**

In `src/adebug.h`, define a small parser contract:

```c
typedef enum AQLDebugTool {
  AQL_DEBUG_TOOL_AQL,
  AQL_DEBUG_TOOL_AQLVM
} AQLDebugTool;

typedef enum AQLDebugParseResult {
  AQL_DEBUG_PARSE_NO_MATCH = 0,
  AQL_DEBUG_PARSE_MATCH = 1,
  AQL_DEBUG_PARSE_ERROR = -1
} AQLDebugParseResult;

AQLDebugParseResult aql_debug_parse_option(AQLDebugTool tool,
                                           const char *arg,
                                           AQLDebugMask *mask,
                                           int *stop_after_lex,
                                           int *stop_after_parse,
                                           int *stop_after_compile);
```

- [ ] **Step 2: Implement parser**

In `src/adebug.c`:

- `-v` enables all user-visible debug channels supported by that tool.
- `-vd` enables detail.
- `-vt` enables VM trace.
- `-vb` enables bytecode.
- `-vl`, `-vast`, `-st`, `-sa`, `-sb` are accepted only for `AQL_DEBUG_TOOL_AQL`.
- `-q` is accepted only for `AQL_DEBUG_TOOL_AQLVM` and clears the mask.

- [ ] **Step 3: Refactor `src/main.c`**

Replace source debug option parsing branches with calls to `aql_debug_parse_option(AQL_DEBUG_TOOL_AQL, ...)`.

Preserve local variables:

```c
int stop_after_lex = 0;
int stop_after_parse = 0;
int stop_after_compile = 0;
```

After parsing, write these into existing global stop flags if current code expects globals.

- [ ] **Step 4: Refactor `src/aqlvm.c`**

Replace VM debug option parsing branches with calls to `aql_debug_parse_option(AQL_DEBUG_TOOL_AQLVM, ...)`.

Do not make `aqlvm` accept source-only flags.

- [ ] **Step 5: Verify**

Run:

```bash
make both
make aqlvm
bash test/debug/debug_flags_smoke.sh
./bin/aqlvm --help
./bin/aql --help
git diff --check
```

Expected:
- Both help texts still list their supported flags.
- `aqlvm -vl` should fail as an unsupported option.
- Existing VM smoke still passes.

- [ ] **Step 6: Commit**

```bash
git add src/adebug.h src/adebug.c src/main.c src/aqlvm.c test/debug/debug_flags_smoke.sh
git commit -m "refactor(debug): share debug option parsing"
```

---

### Task 3: Move VM Trace Formatting Out Of `adebug.h`

**Files:**
- Modify: `src/adebug.h`
- Modify: `src/avm.h`
- Modify: `src/avm_debug.c`
- Modify: `src/avm_core.c`
- Test: `test/vm_harness_smoke.sh`
- Test: `test/vm/bytecode/basic/control_flow/jump.by`

- [ ] **Step 1: Write focused trace expectation**

Extend `test/debug/debug_flags_smoke.sh` with one stable assertion:

```bash
trace_output="$("$repo_root/bin/aqlvm" -vt "$tmp_by")"
grep -q "PC=" <<<"$trace_output"
grep -q "LOADI" <<<"$trace_output"
grep -q "RETURN1" <<<"$trace_output"
```

- [ ] **Step 2: Add VM hook declarations**

In `src/avm.h`, declare debug-build hooks:

```c
#ifdef AQL_DEBUG_BUILD
void aql_debug_vm_before(aql_State *L, CallInfo *ci, const Instruction *pc,
                         Instruction i, LClosure *cl, StkId base);
void aql_debug_vm_after(aql_State *L, CallInfo *ci, const Instruction *pc,
                        Instruction i, LClosure *cl, StkId base);
#define AQL_VM_DEBUG_BEFORE(L, ci, pc, i, cl, base) \
  aql_debug_vm_before((L), (ci), (pc), (i), (cl), (base))
#define AQL_VM_DEBUG_AFTER(L, ci, pc, i, cl, base) \
  aql_debug_vm_after((L), (ci), (pc), (i), (cl), (base))
#else
#define AQL_VM_DEBUG_BEFORE(L, ci, pc, i, cl, base) ((void)0)
#define AQL_VM_DEBUG_AFTER(L, ci, pc, i, cl, base) ((void)0)
#endif
```

- [ ] **Step 3: Implement VM hooks**

In `src/avm_debug.c`, implement hooks with explicit parameters. Do not use global `current_L/current_ci/current_i/current_pc/current_cl/current_base`.

Keep formatting simple and stable first:

```text
PC=0 [main] LOADI BEFORE: A=R0 sBx=42
PC=0 [main] LOADI AFTER: A=R0=42 sBx=42
```

- [ ] **Step 4: Replace call sites**

In `src/avm_core.c`, replace direct `aql_vt_set_context` and ad hoc instruction-specific `DEBUG_*` calls with `AQL_VM_DEBUG_BEFORE` and `AQL_VM_DEBUG_AFTER` at stable dispatch boundaries.

If an opcode has special trace needs, add formatting inside `avm_debug.c`, not in `avm_core.c`.

- [ ] **Step 5: Remove VM trace macros from `adebug.h`**

Delete large VM-specific macro blocks from `src/adebug.h`, including:

- `AQL_VT_BEFORE`
- `AQL_VT_AFTER`
- `DEBUG_GET_REG_VALUE`
- `DEBUG_GET_CONST_VALUE`
- `DEBUG_GETUPVAL`
- `DEBUG_SETUPVAL`
- `DEBUG_MULK`
- other opcode-specific `DEBUG_*` macros

Leave only generic debug macros in `adebug.h`.

- [ ] **Step 6: Verify**

Run:

```bash
make both
make aqlvm
bash test/debug/debug_flags_smoke.sh
bash test/vm_harness_smoke.sh
./bin/vmtest test/vm/bytecode ./bin/aqlvm -v
git diff --check
```

Expected:
- Full bytecode VM suite remains 147/147.
- `make both` still passes.
- No release build debug macro leakage.

- [ ] **Step 7: Commit**

```bash
git add src/adebug.h src/avm.h src/avm_debug.c src/avm_core.c test/debug/debug_flags_smoke.sh
git commit -m "refactor(debug): move VM trace formatting to avm_debug"
```

---

### Task 4: Separate Stable Output From Pretty Output

**Files:**
- Modify: `src/adebug.h`
- Modify: `src/adebug.c`
- Modify: `src/main.c`
- Modify: `src/aqlvm.c`
- Modify: `docs/debug-req.md`
- Test: `test/debug/debug_flags_smoke.sh`

- [ ] **Step 1: Add output mode**

In `src/adebug.h`:

```c
typedef enum AQLDebugFormat {
  AQL_DEBUG_FORMAT_PLAIN,
  AQL_DEBUG_FORMAT_PRETTY
} AQLDebugFormat;

void aql_debug_set_format(AQLDebugFormat format);
AQLDebugFormat aql_debug_get_format(void);
```

- [ ] **Step 2: Default to plain for stable tests**

In `src/adebug.c`, make default output plain ASCII. Keep pretty output available for human CLI use.

Do not remove existing pretty strings in this task if doing so is high-risk; gate them behind `AQL_DEBUG_FORMAT_PRETTY`.

- [ ] **Step 3: Add CLI option**

Support:

```text
--debug-format=plain
--debug-format=pretty
```

Accept it in both `aql` and `aqlvm`.

- [ ] **Step 4: Update smoke test**

In `test/debug/debug_flags_smoke.sh`, force plain output:

```bash
"$repo_root/bin/aqlvm" --debug-format=plain -vt "$tmp_by" | grep -q "PC="
```

- [ ] **Step 5: Verify**

Run:

```bash
make both
bash test/debug/debug_flags_smoke.sh
./bin/aqlvm --debug-format=pretty -vt test/vm/bytecode/basic/load_store/loadi.by
git diff --check
```

Expected:
- Plain output is stable enough for grep-based tests.
- Pretty output remains available.

- [ ] **Step 6: Commit**

```bash
git add src/adebug.h src/adebug.c src/main.c src/aqlvm.c docs/debug-req.md test/debug/debug_flags_smoke.sh
git commit -m "feat(debug): separate plain and pretty output modes"
```

---

### Task 5: Collapse `adebug_user.*` Into Compatibility Or Remove It

**Files:**
- Modify: `src/adebug_user.h`
- Modify: `src/adebug_user.c`
- Modify: `src/alex.c`
- Modify: `src/main.c`
- Modify: `src/aqlvm.c`
- Modify: `src/aparser.c`
- Test: `test/debug/debug_flags_smoke.sh`

- [ ] **Step 1: Find remaining dependencies**

Run:

```bash
rg -n "adebug_user|AQL_DEBUG_|aqlD_" src test
```

Expected: List all old API users.

- [ ] **Step 2: Migrate call sites**

Replace direct `AQL_DEBUG_*` flag use with canonical `AQL_DBG_*`.

Replace user-visible `aqlD_*` print functions with `aql_debug_*` equivalents.

Do not touch `ado.h` `aqlD_*` symbols; those are non-debug runtime functions.

- [ ] **Step 3: Reduce compatibility header**

If no production source needs `adebug_user.h`, make it a thin compatibility wrapper:

```c
#ifndef adebug_user_h
#define adebug_user_h
#include "adebug.h"
#endif
```

If nothing includes it, delete `src/adebug_user.h` and `src/adebug_user.c`, and remove `src/adebug_user.c` from `CORE_SOURCES` in `Makefile`.

- [ ] **Step 4: Verify**

Run:

```bash
make both
make aqlvm
bash test/debug/debug_flags_smoke.sh
bash test/vm_harness_smoke.sh
./bin/vmtest test/vm/bytecode ./bin/aqlvm -v
git diff --check
```

Expected:
- No user-visible behavior regression.
- `rg "AQL_DEBUG_" src` only returns compatibility definitions or no results.

- [ ] **Step 5: Commit**

```bash
git add src Makefile test/debug/debug_flags_smoke.sh
git commit -m "refactor(debug): retire duplicate user debug API"
```

---

### Task 6: Isolate Internal Developer Diagnostics

**Files:**
- Modify: `src/adebug_internal.h`
- Modify: `src/adebug_internal.c`
- Modify: any source that includes `adebug_internal.h`
- Test: existing build and VM suites

- [ ] **Step 1: Rename conflicting internal macros**

In `src/adebug_internal.h`, rename:

- `AQL_DEBUG` -> `AQL_INTERNAL_DEBUG`
- `AQL_TRACE` -> `AQL_INTERNAL_TRACE`
- `AQL_PROFILE_START` -> `AQL_INTERNAL_PROFILE_START`
- `AQL_PROFILE_END` -> `AQL_INTERNAL_PROFILE_END`
- `AQL_ASSERT` -> `AQL_INTERNAL_ASSERT`

Keep temporary aliases for one task if needed, but mark them as compatibility-only.

- [ ] **Step 2: Check naming collisions**

Run:

```bash
rg -n "#define AQL_DEBUG|AQL_PROFILE_START|AQL_TRACE|AQL_ASSERT" src
```

Expected: Only internal names remain, or old names appear only in compatibility comments.

- [ ] **Step 3: Verify**

Run:

```bash
make both
bash test/debug/debug_flags_smoke.sh
./bin/vmtest test/vm/bytecode ./bin/aqlvm -v
git diff --check
```

Expected:
- Build and VM regression pass.
- Public debug flags still behave as before.

- [ ] **Step 4: Commit**

```bash
git add src/adebug_internal.h src/adebug_internal.c
git commit -m "refactor(debug): isolate internal diagnostics"
```

---

### Task 7: Update Documentation And Agent Guidance

**Files:**
- Modify: `docs/debug-design.md`
- Modify: `docs/debug-req.md`
- Modify: `AGENTS.md`

- [ ] **Step 1: Document final layering**

Update docs to describe:

```text
adebug           = public debug facade, CLI flags, sinks, output formats
avm_debug        = VM instruction trace formatting
adebug_internal  = developer-only diagnostics
```

- [ ] **Step 2: Document supported CLI matrix**

Add a small matrix:

```text
flag   aql   aqlvm   meaning
-vl    yes   no      lexer tokens
-vast  yes   no      AST
-vb    yes   yes     bytecode/code view
-vt    yes   yes     VM trace
-vd    yes   yes     detailed debug
-v     yes   yes     all supported channels for the tool
-q     no    yes     quiet bytecode runner
```

- [ ] **Step 3: Update standard verification commands**

In `AGENTS.md`, include:

```bash
make both
bash test/debug/debug_flags_smoke.sh
bash test/vm_harness_smoke.sh
./bin/vmtest test/vm/bytecode ./bin/aqlvm -v
git diff --check
```

- [ ] **Step 4: Verify docs references**

Run:

```bash
rg -n "AQL_DEBUG_|AQL_FLAG_|aqlD_|adebug_user|aqlm" docs AGENTS.md
```

Expected:
- Legacy names appear only when explicitly described as compatibility or historical.
- `aqlm` appears only as compatibility alias if documented.

- [ ] **Step 5: Commit**

```bash
git add docs/debug-design.md docs/debug-req.md AGENTS.md
git commit -m "docs(debug): document debug layering"
```

---

## Final Verification

Run all of these before declaring the refactor complete:

```bash
make both
make aqlvm
bash test/debug/debug_flags_smoke.sh
bash test/vm_harness_smoke.sh
make -C test/vm vmtest
./bin/vmtest test/vm/bytecode ./bin/aqlvm -v
make test_ivabc_55
make test_tbc_close_55
make test_runtime_error_55
make test_metamethod_le_55
git diff --check
```

Expected final state:

- `aql` and `aqlvm` share one canonical debug flag state.
- `adebug.h` no longer contains VM opcode formatting macros.
- VM trace formatting lives in `avm_debug.c`.
- Release build debug hooks compile away.
- `adebug_user.*` is either gone or only a thin compatibility wrapper.
- `adebug_internal.*` is isolated from user-facing debug output.
- Full VM bytecode suite remains green.

---

## Known Risks

- `main.c` currently uses `AQL_DEBUG_*`, while `aqlvm.c` uses `AQL_FLAG_*`; migrate through compatibility aliases first, not by deleting either side immediately.
- `aqlD_*` names collide conceptually with Lua-style `do` runtime functions such as those in `ado.h`; do not blindly rename every `aqlD_*` occurrence.
- VM trace output may be used informally by tests or debugging habits. Keep the first stable plain format minimal before changing pretty output.
- Do not combine this refactor with Lua 5.5 opcode or VM semantic changes. Debug refactor should preserve runtime behavior.
