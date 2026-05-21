# Claude Guide

Follow `AGENT.md` for the project-specific engineering guide. The short version:
Voyager is a PHP C extension that rewrites Zend function and method table
entries at runtime, so treat changes as memory-sensitive Zend engine work.

## Default Workflow

1. Read `README.md`, `AGENT.md`, and the touched C file before editing.
2. Check `git status --short`; preserve unrelated user changes.
3. Make the smallest change that preserves existing API semantics.
4. Add focused PHPT coverage for behavioral changes.
5. Verify with the relevant PHPT test command, and run the full `tests`
   directory when feasible.

## High-Risk Areas

- `voyager_functions.c`: function cloning, runtime cache clearing, stack-size
  fixes, request restore, and reflection invalidation.
- `voyager_methods.c`: method flags, `static` preservation, inheritance
  propagation, magic method pointers, and request restore.
- `voyager.h`: Zend compatibility shims, module globals, inline parsers, and
  reflection struct mirrors.
- `voyager.stub.php` plus `voyager_arginfo.h`: update together when exported
  signatures change; `voyager_arginfo.h` is generated.

## Commands

```bash
phpize
./configure --enable-voyager
make
php run-tests.php -d extension=modules/voyager.so tests
```

Use targeted PHPT runs while iterating, for example:

```bash
php run-tests.php -d extension=modules/voyager.so tests/static_method_redefine.phpt
```

## Non-Negotiables

- Do not hand-wave memory ownership. Audit every new reference, clone, and
  destructor path.
- Do not skip cache invalidation or reflection cleanup around function/method
  replacement.
- Do not break request-scoped restoration in `RSHUTDOWN`.
- Do not edit generated or build-output files as a side effect.
- Do not treat CMake as the primary build system; use the PHP extension build
  flow unless the user specifically asks about CMake.
