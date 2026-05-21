# Agent Guide

Voyager is a PHP C extension for runtime debugging. It replaces existing PHP
functions and user-defined class methods at runtime by cloning Zend functions
and swapping entries in Zend hash tables. Treat this repository as low-level
runtime code: small changes can affect request isolation, reflection objects,
opcache/runtime caches, inheritance, and memory ownership.

## Project Map

- `voyager.c` owns module lifecycle, INI registration, request-scoped restore
  tables, and `RINIT`/`RSHUTDOWN` cleanup.
- `voyager_functions.c` implements `voyager_function_redefine()`, lambda
  generation, function cloning/destruction, runtime cache clearing, hardcoded
  stack-size fixes, request restore, and reflection invalidation.
- `voyager_methods.c` implements `voyager_method_redefine()`, method flag
  handling, child-class propagation, magic method pointer updates, and method
  request restore.
- `voyager_common.c` centralizes Zend magic method pointer maintenance.
- `voyager.h` contains shared declarations, compatibility helpers, module
  globals, argument parsers, reflection struct mirrors, and inline utilities.
- `voyager.stub.php` is the source for exported function signatures.
- `voyager_arginfo.h` is generated from `voyager.stub.php`; do not hand-edit it
  unless the user explicitly asks for a generated-file patch.
- `tests/*.phpt` are PHP extension tests. Add focused PHPT coverage for behavior
  changes.

## Build And Test

Primary build flow:

```bash
phpize
./configure --enable-voyager
make
```

Run tests with the built extension:

```bash
php run-tests.php -d extension=modules/voyager.so tests
```

Useful one-off checks:

```bash
php -d extension=modules/voyager.so -m | grep voyager
php -d extension=modules/voyager.so your_script.php
```

Regenerate arginfo after changing `voyager.stub.php` with the PHP generator
available in the local PHP source/tooling setup, then inspect the generated
diff. Keep generated output minimal and committed with the stub change.

`CMakeLists.txt` exists, but the authoritative extension build path is
`phpize`/`configure`/`make` from `config.m4`.

## Coding Rules

- Preserve Zend memory ownership rules. Match `emalloc`/`efree`,
  `pemalloc`/`pefree`, and `zend_string_addref`/release patterns already used in
  adjacent code.
- Before replacing a function or method, keep the call-stack guard intact so
  active functions/methods cannot be redefined.
- After replacing or restoring functions/methods, clear relevant runtime caches.
  Function replacement also updates hardcoded call stack sizes.
- Keep request-scoped behavior intact: redefinitions are recorded on first
  change during a request and restored in `RSHUTDOWN`.
- When changing method replacement, preserve default behavior for original
  visibility, `static`, and return-by-reference flags unless callers explicitly
  override flags.
- When touching methods, update magic method pointers and child class
  propagation together. Parent changes must continue to affect inherited child
  methods without clobbering child overrides.
- When replacing or restoring a `zend_function`, invalidate stale
  `ReflectionFunction`, `ReflectionMethod`, and `ReflectionParameter` references.
- Be careful with temporary eval artifacts:
  `__voyager_temporary_function__`, `__voyager_temporary_class__`, and
  `__voyager_temporary_method__` must be cleaned up on success and failure
  paths where applicable.
- Keep C style consistent with the existing files: 4-space indentation, K&R-ish
  braces, existing `/* {{{ */` region comments, and no broad formatting churn.

## API Semantics To Preserve

- `voyager_function_redefine()` accepts either a closure or string argument list
  plus string body.
- Function string-body mode preserves return-by-reference by default when the
  original function returns by reference; an explicit boolean overrides it.
- `voyager_method_redefine()` accepts either a closure or string argument list
  plus string body.
- Method redefinition is limited to user-defined classes and user functions.
- Method string-body mode preserves original visibility, `static`, and
  return-by-reference flags by default; explicit flags are authoritative.
- `voyager.internal_override=1` is required before redefining internal PHP
  functions.

## Testing Expectations

For behavior changes, add or update PHPT tests that exercise the public API
rather than only internal helpers. Prefer narrowly named tests that cover:

- closure mode and string-body mode when both paths are affected;
- inherited methods and child overrides for method propagation changes;
- static methods, visibility, and return-by-reference flags when method flags
  change;
- request restoration behavior when lifecycle code changes;
- reflection behavior when replaced functions or methods are involved;
- warnings and `false` returns for invalid inputs or unsupported targets.

Run the relevant PHPT files first, then the full `tests` directory when feasible.

## Git And Workspace Hygiene

- The workspace may contain generated build output (`Makefile`, `.libs/`,
  `modules/`, `build/`, `run-tests.php`, `config.status`, etc.). Do not clean or
  delete these unless the user asks.
- Do not revert unrelated changes. Check `git status --short` before editing and
  keep patches scoped to the requested files.
- Avoid broad refactors in this codebase. Runtime extension changes should be
  small, explicit, and backed by tests.
