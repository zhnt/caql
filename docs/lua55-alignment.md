# Lua 5.5.0 Alignment Policy

## Goal

AQL uses Lua 5.5.0 as the baseline for VM and runtime semantics. AQL is not a
plain Lua clone: containers, static types, class/self/method syntax, and future
AI workflow syntax remain first-class AQL extensions. The rule is that an
extension must be explicit and must not silently change behavior already defined
by Lua 5.5.0.

The current local Lua reference lives in the workspace sibling directory:
`../lua5.5`.

## Baseline Surface

These areas should follow Lua 5.5.0 unless a document explicitly marks a
different AQL semantic choice:

- VM opcode behavior, including instruction operands, open result conventions,
  `CALL`, `TAILCALL`, `RETURN`, jumps, `SETLIST`, and varargs.
- Function calls, stack frames, closures, upvalues, open upvalue closing, and
  tail calls.
- Table behavior, including `GETTABLE`, `SETTABLE`, `GETFIELD`, `SETFIELD`,
  `GETI`, `SETI`, and metatable lookup rules.
- Metamethod dispatch for Lua-defined operations: arithmetic, bitwise,
  comparison, length, concat, index/newindex, call, and close.
- Runtime error behavior for invalid operations and missing metamethods.
- Iterator and close semantics, including `TFORPREP`, `TFORCALL`, `TFORLOOP`,
  `TBC`, and `__close`.
- Yield and finish-op recovery behavior where AQL supports coroutine-like
  execution.

## AQL Extension Surface

These features are intentionally outside Lua and should be treated as AQL
extensions:

- Containers: array, slice, dict, vector, range, and future specialized data
  structures.
- Container bytecodes such as `NEWOBJECT`, `GETPROP`, `SETPROP`, `INVOKE`, and
  future vector/SIMD instructions.
- Static type annotations, inference, and type-directed optimization.
- Future `class`, `self`, and `method` syntax sugar.
- Future AI/agentic language constructs such as provider, model/session,
  context, tools, steps, and streaming.
- JIT/AOT, SIMD, and data-science-oriented runtime extensions.

AQL extensions should reuse Lua mechanisms where practical. For example,
containers may have metatables and may implement `__index`, `__newindex`,
`__len`, `__eq`, `__lt`, `__le`, and arithmetic metamethods, but their behavior
must be described as container behavior rather than Lua table behavior.

## Conflict Rule

When Lua 5.5.0 defines behavior, AQL follows Lua 5.5.0 by default. If AQL needs
a different behavior, the difference must be documented and tested as an AQL
semantic choice.

Example: Lua 5.5.0 `<=` uses `__le` directly and does not fall back to reverse
`__lt`. AQL therefore follows that behavior in core comparison. Containers may
still define their own `__le`, but missing `__le` must not silently call `__lt`
under the Lua-compatible path.

## Testing Policy

Tests should be split by intent:

- Lua-alignment tests verify behavior against Lua 5.5.0 source and tests.
- AQL-extension tests verify containers, typed operations, and future syntax.
- Regression tests should name the semantic target when behavior is subtle,
  such as `metamethod_le_55_test`.

When a Lua test cannot be ported directly because AQL lacks syntax or library
surface, add the smallest VM or C-level fixture that exercises the same runtime
rule.

## Current VM Status

- Core opcode numbering, names, and opmodes in `src/aopcodes.h` are aligned to
  Lua 5.5.0 through `OP_EXTRAARG`.
- Text bytecode parsing distinguishes Lua 5.5.0 key operands from value
  operands: `GETTABUP`/`GETFIELD` keep key constants in their plain operand
  fields, while `SETTABUP`, `SETTABLE`, `SETI`, and `SETFIELD` encode constant
  values through the separate `k` bit. `SELF` also treats C as `K[C]`, matching
  Lua. The source code generator emits the corresponding Lua table
  getter/setter family (`GETTABLE`/`SETTABLE`, `GETI`/`SETI`,
  `GETFIELD`/`SETFIELD`, and `GETTABUP`/`SETTABUP`) instead of collapsing
  indexed writes into `SETTABUP`.
- The core instruction format enum includes Lua 5.5.0 `ivABC`; `OP_NEWTABLE`
  and `OP_SETLIST` use Lua-compatible `vB/vC` fields, including
  `EXTRAARG _ vC` composition when the `k` bit is set. Coverage lives in
  `test/vm/ivabc_55_test.c` and the `setlist_ivabc_*` bytecode fixtures under
  `test/vm/bytecode/basic/table`.
- AQL-only bytecodes start after the Lua core opcode range, so container and
  builtin extensions no longer collide with Lua 5.5.0 `OP_GETVARG` and
  `OP_ERRNNIL`.
- `OP_GETVARG` and `OP_ERRNNIL` have minimal VM bytecode coverage under
  `test/vm/bytecode/basic/vararg`.
- Fixed-result `OP_VARARG` follows Lua result semantics instead of returning an
  AQL container for the single-result case.
- `TFORPREP`, `TFORCALL`, and `TFORLOOP` now use Lua 5.5.0's generic-for
  register layout: `TFORPREP` swaps the control and closing slots, iterator
  calls start at `R[A+3]`, and `TFORLOOP` tests the first returned value in
  `R[A+3]`. Minimal coverage lives in
  `test/vm/bytecode/basic/control_flow/tfor_lua55_layout.by`.
- `OP_TBC`, `OP_CLOSE`, and the `TFORPREP` closing slot now share Lua 5.5.0
  to-be-closed behavior: nil/false closing values are no-ops, non-closable
  values raise runtime errors, `__close` methods run in reverse registration
  order, and error objects are passed to `__close` on error paths. C-level
  coverage also checks protected close recovery continuing after a closing
  error. These tests live in `test/vm/tbc_close_55_test.c`; bytecode-level nil
  `TBC` coverage lives in `test/vm/bytecode/basic/close/tbc_nil_noop.by`.
- AQL extension dispatch now includes `DIVI`, `ITER_INIT`, `ITER_NEXT`, and
  `CALLBUILTIN` so bytecode execution no longer silently skips these opcodes.
- Runtime error helpers `aqlG_runerror`, `aqlG_typeerror`, and
  `aqlG_ordererror` now throw `AQL_ERRRUN` through the protected-call machinery
  instead of returning to the VM. They also leave a string error object on the
  stack, and `aqlD_pcall` moves that object to the protected-call `oldtop`.
  Type names used in these messages now follow AQL's public tag numbering
  (`AQL_TSTRING`, `AQL_TTABLE`, containers, builtins, and ranges), avoiding
  Lua-style diagnostics that report the wrong value category.
  Memory errors reuse the precreated `G(L)->memerrmsg` string, following Lua's
  allocation-error path instead of allocating a fresh message while handling
  `AQL_ERRMEM`.
  When the active frame is an AQL closure with debug source/line information,
  `aqlG_runerror` prefixes messages with `source:line`, matching Lua's
  diagnostic shape.
  Minimal coverage lives in
  `test/vm/runtime_error_55_test.c`.
- Hidden vararg frames now follow Lua 5.5.0's `VARARGPREP`/`VARARG`/`GETVARG`
  shape. Vararg functions adjust the call frame before execution, retrieve
  fixed and open vararg results from hidden slots, and restore `ci->func` on
  `RETURN`/`TAILCALL`. Minimal coverage lives under
  `test/vm/bytecode/basic/vararg` and
  `test/regression/functions/func_varargs_sum.aql`.
- Missing arithmetic, bitwise, and concat metamethods now raise runtime errors
  instead of silently producing `nil`, matching Lua 5.5.0's
  `luaT_trybinTM` behavior. Coverage lives in
  `test/vm/metamethod_missing_55_test.c`.

## Current Priorities

1. Improve Lua-compatible diagnostics by adding variable/function context
   (`varinfo`, object names, and call-site names) to type and order errors.
2. Complete metatable and metamethod coverage with tests for missing-method
   errors and successful dispatch.
3. Continue validating varargs and open-result alignment against additional
   source-level and bytecode-level fixtures.
4. Keep container and future AQL syntax work on top of the Lua-aligned runtime
   instead of replacing the runtime rules.
