# Lua 5.5.1 Alignment Policy

## Goal

AQL uses Lua 5.5.1 as the baseline for VM and runtime semantics. AQL is not a
plain Lua clone: containers, static types, class/self/method syntax, and future
AI workflow syntax remain first-class AQL extensions. The rule is that an
extension must be explicit and must not silently change behavior already defined
by Lua 5.5.1.

The current local Lua reference lives in the workspace sibling directory:
`../lua`.

## Baseline Surface

These areas should follow Lua 5.5.1 unless a document explicitly marks a
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

When Lua 5.5.1 defines behavior, AQL follows Lua 5.5.1 by default. If AQL needs
a different behavior, the difference must be documented and tested as an AQL
semantic choice.

Example: Lua 5.5.1 `<=` uses `__le` directly and does not fall back to reverse
`__lt`. AQL therefore follows that behavior in core comparison. Containers may
still define their own `__le`, but missing `__le` must not silently call `__lt`
under the Lua-compatible path.

## Testing Policy

Tests should be split by intent:

- Lua-alignment tests verify behavior against Lua 5.5.1 source and tests.
- AQL-extension tests verify containers, typed operations, and future syntax.
- Regression tests should name the semantic target when behavior is subtle,
  such as `metamethod_le_55_test`.

When a Lua test cannot be ported directly because AQL lacks syntax or library
surface, add the smallest VM or C-level fixture that exercises the same runtime
rule.

## Current Priorities

1. Implement real runtime errors for `aqlG_runerror`, `aqlG_typeerror`, and
   `aqlG_ordererror`; many Lua 5.5 behaviors depend on errors rather than false
   results.
2. Complete metatable and metamethod coverage with tests for missing-method
   errors and successful dispatch.
3. Align `TFORPREP`, `TFORCALL`, `TFORLOOP`, `TBC`, and `__close` with Lua 5.5.1.
4. Align varargs, open results, and any missing Lua 5.5 opcodes used by the
   local reference.
5. Keep container and future AQL syntax work on top of the Lua-aligned runtime
   instead of replacing the runtime rules.
