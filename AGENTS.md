# AGENTS.md

Guidance for coding agents working in this repository.

## Project Direction

AQL uses Lua 5.5.1 as the baseline for VM and runtime semantics, while keeping
AQL containers, static typing, class/self/method syntax, and future AI workflow
syntax as explicit extensions.

Read `docs/lua55-alignment.md` before changing VM, runtime, bytecode,
metamethod, call, closure, table, iterator, or error behavior.

The local Lua reference source is the workspace sibling directory:
`../lua`.

## Alignment Rules

- Follow Lua 5.5.1 for core VM/runtime behavior unless an AQL semantic choice is
  explicitly documented.
- Do not silently change Lua-defined behavior to support AQL extensions.
- Keep AQL extensions visible as extensions: containers, `NEWOBJECT`,
  `GETPROP`, `SETPROP`, `INVOKE`, vector/SIMD operations, static types,
  `class/self/method`, and future agentic syntax.
- Prefer reusing Lua mechanisms for extensions where practical, especially
  metatables and metamethods.
- If local behavior differs from Lua 5.5.1, add or update documentation and add
  a regression test naming the semantic target.

## Current VM Priorities

1. Implement real runtime errors for `aqlG_runerror`, `aqlG_typeerror`, and
   `aqlG_ordererror`.
2. Complete metatable and metamethod coverage, including missing-method errors.
3. Align `TFORPREP`, `TFORCALL`, `TFORLOOP`, `TBC`, and `__close` with Lua 5.5.1.
4. Align varargs, open results, and any missing Lua 5.5.1 opcode behavior.
5. Keep container and future AQL syntax work layered on top of the Lua-aligned
   runtime.

## Repository Layout

- `src/`: core C implementation.
- `src/avm_core.c`: main VM execution loop.
- `src/aobject.c`: generic object operations and metamethod helpers.
- `src/ado.c`: call/return support.
- `src/afunc.c`: closures and upvalues.
- `src/atable.c`: table implementation.
- `test/vm/bytecode/`: text bytecode fixtures run by `bin/vmtest`.
- `test/vm/*.c`: focused C-level VM/runtime regression tests.
- `docs/lua55-alignment.md`: Lua 5.5.1 baseline and AQL extension policy.

## Build And Test

Use the project root as the working directory:

```bash
cd /home/dev/aqlworkspace/caql
```

Common checks:

```bash
make aqlm
./bin/vmtest test/vm/bytecode ./bin/aqlm -v
make test_metamethod_le_55
```

Generated smoke tests:

```bash
make -C test/vm vmtest
```

For targeted VM work, add the smallest fixture that proves the runtime rule:

- Use `.by` bytecode fixtures when the bytecode format can express the case.
- Use a focused C test under `test/vm/` when the fixture needs direct runtime
  construction, such as custom metatables.

## Coding Rules

- Keep edits scoped to the requested behavior.
- Use C11 and the existing Lua-style naming conventions.
- Prefer existing helpers and object macros over new ad hoc access patterns.
- Do not introduce C++.
- Do not rewrite unrelated files or reformat broad sections.
- Preserve user or generated work in the tree; never revert unrelated changes.
- When behavior changes, update tests first or alongside the implementation.

## Git Workflow

- Check `git status --short` before editing and before committing.
- Commit focused changes with clear messages.
- Use SSH remote for pushes: `git@github.com:zhnt/caql.git`.
- Do not amend or rewrite history unless explicitly requested.
- If `git push` fails due to authentication or network restrictions, report the
  exact error and leave local commits intact.

## Communication Notes

- Prefer concise engineering updates.
- State which verification commands were run and whether they passed.
- When comparing with Lua, cite the local reference file and behavior, for
  example `../lua/lvm.c` for VM execution semantics.
