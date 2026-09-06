# C standard

These rules apply to C development for the GEM UI port to Linux.
All development and debugging happens locally on Linux with GCC.
Use full warnings and sanitizers while debugging.

## Naming Conventions

- Use lowercase snake_case for C symbols.
- Do not use Hungarian notation, PascalCase, or camelCase.
- Use uppercase only when a true macro-style constant makes sense.
- Use lowercase snake_case for file names.

Examples:

- Good: `screen_clear()`, `player_score`, `dbf_open()`
- Bad: `ScreenClear()`, `playerScore`, `DbfOpen()`

## Header and Implementation Separation

Every module must follow a classic C layout.

- A public `.h` file in `include/` (or `samples/include/` for sample
  interfaces) contains:
  - A file header comment
  - Public declarations
  - Public types
  - Public constants or macros
  - Documentation for every public function
- A `.c` implementation file (in `src/`, `lib/<name>/`, or `samples/src/` and `samples/lib/`
  for sample applications) contains:
  - A file header comment
  - `#include "module.h"`
  - Private static helpers and state
  - The implementation

Do not put function bodies in headers unless a tiny static helper is
truly necessary and clearly marked.

## File Header

Every `.c` and `.h` file must begin with a real file header in this
style:

```c
/*
 * Describe exactly what this file does.
 * Be specific about the module purpose, important design choices,
 * and any hardware assumptions that matter.
 *
 * MIT License (see: LICENSE)
 * Copyright (C) 2026 tomaz stih
 */
```

Rules:

- Replace the placeholder text with a real description.
- The comment must describe the file, not just list its symbols.
- Keep lines readable on an 80-column terminal where practical.

## Function Documentation

Every function declared in a public header must have a comment
immediately above its prototype.

Example:

```c
/*
 * Clears the screen and resets the cursor to (0,0).
 * Uses hardware scrolling when the target supports it.
 * Returns nothing.
 */
void screen_clear(void);
```

Rules:

- Say what the function does.
- Mention parameters and the return value when needed.
- Mention side effects, limits, or hardware requirements when relevant.
- Document private static helpers too when their purpose is not obvious.

## General Coding Rules

- Use portable C11 features that build cleanly with GCC.
- Fix all warnings under `-Wall -Wextra -pedantic`.
- Prefer small, focused functions.
- Keep source files under about 500 lines when possible.
- Avoid global variables unless they are truly necessary.
- Keep code readable on an 80-column terminal.
- Use 4 spaces for indentation and no tabs.
- Use K&R braces for functions. Control-flow brace style may vary
  within reason, but keep it consistent inside a file.

## Build and verification

- Always build and test with GCC first.
- Use `-Wall -Wextra -pedantic` and fix all warnings.
- Use `-g -fsanitize=address,undefined` for hosted debug builds.
- Use a simple in-project test framework rather than an external one.
- Keep VS Code `F5` debugging working through `.vscode/`.
- Follow the [directory standard](DIRECTORIES_STANDARD.md) for output
  locations, makefile organization and test data.

Test-only helper headers belong in `tests/include/`, as specified by the
directory standard. Upstream vendor sources retain their own conventions;
project-written build integration follows the project standards.

## Compatibility and automated checks

Keep public Atari GEM identifiers, structures, field names, constants and API
signatures source-compatible, including `WORD`, `LONG`, `OBJECT` and `MFDB`.
These established public names are compatibility contracts, not a naming model
for new private code. Private project helpers use lowercase snake_case without
reserved leading underscores. POSIX feature macros and compiler-defined names
retain their required spellings. Upstream Musashi and historical Atari sample
data retain their original licenses, symbols and formatting; document local
security patches separately.

Use `.clang-format` with Clang Format 18 for owned C/header layout. `make
standards` checks the mechanically enforceable rules across the full owned
source inventory; review semantics, comment accuracy, module boundaries and
file-size guidance separately. Regenerated RPC sources must pass the same
formatting and standards checks as handwritten sources.
