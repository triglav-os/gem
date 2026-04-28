# AGENTS.md - C Coding Standards and Project Rules

This document defines the rules that all agents, including AI agents,
must follow when writing, reviewing, or modifying code in this project.

We are developing C11 code for the GEM UI port from Digital Express
to Linux.

- All development and debugging happens locally on Linux with GCC.
- GCC builds must use full warnings and sanitizers while debugging.

## 1. Project Directory Structure

Do not create files outside the approved project directories.
Do not leave editor backups, stray object files, or temporary files in
the tree.

- `src/` contains application `.c` files.
- `include/` contains all public `.h` files.
- `lib/` contains project libraries and third-party libraries.
- `lib/<name>/` contains the implementation of a library such as
  `lib/dbf/`.
- `bin/` contains final programs and other final generated outputs.
- `build/` contains all build artifacts, including object files,
  dependency files, static libraries, maps, listings, and temporary
  build products.
- `docs/` contains project notes and design documents.
- `tests/` contains automated tests.
- `tests/data/` contains data files used by tests.

Rules:

- Public headers must live in `include/`.
- Project source files may live in `src/` or in a library directory
  under `lib/`.
- Build artifacts must go only into the root `build/` directory.
- No nested `build/` directories are allowed.
- Nothing else should be created at the project root except normal
  project files such as `LICENSE`, `README.md`, `Makefile`,
  `AGENTS.md`, and similar top-level metadata.

## 2. Naming Conventions

- Use lowercase snake_case for C symbols.
- Do not use Hungarian notation, PascalCase, or camelCase.
- Use uppercase only when a true macro-style constant makes sense.
- Use lowercase snake_case for file names.

Examples:

- Good: `screen_clear()`, `player_score`, `dbf_open()`
- Bad: `ScreenClear()`, `playerScore`, `DbfOpen()`

## 3. Header and Implementation Separation

Every module must follow a classic C layout.

- A public `.h` file in `include/` contains:
  - A file header comment
  - Public declarations
  - Public types
  - Public constants or macros
  - Documentation for every public function
- A `.c` file in `src/` or `lib/<name>/` contains:
  - A file header comment
  - `#include "module.h"`
  - Private static helpers and state
  - The implementation

Do not put function bodies in headers unless a tiny static helper is
truly necessary and clearly marked.

## 4. File Header

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

## 5. Function Documentation

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

## 6. General Coding Rules

- Use portable C11 features that build cleanly with GCC.
- Fix all warnings under `-Wall -Wextra -pedantic`.
- Prefer small, focused functions.
- Keep source files under about 500 lines when possible.
- Avoid global variables unless they are truly necessary.
- Keep code readable on an 80-column terminal.
- Use 4 spaces for indentation and no tabs.
- Use K&R braces for functions. Control-flow brace style may vary
  within reason, but keep it consistent inside a file.

## 7. Build Rules

- Always build and test with GCC first.
- Use `-g -fsanitize=address,undefined` for hosted debug builds.
- All build outputs must go into the root `build/` directory only.
- Do not create `build/` directories inside `src/`, `lib/`, `tests/`,
  or any other subdirectory.
- Use nested makefiles.
- The top-level `Makefile` should define the main entry points.
- Subdirectories such as `src/`, `lib/`, `lib/dbf/`, and `tests/`
  should have their own makefiles when needed.
- Keep VS Code `F5` debugging working through `.vscode/`.

## 8. Documentation

- Keep `README.md` up to date.
- `README.md` must explain:
  - How to build the project
  - Dependencies
  - Usage
- Keep notes in `docs/` so the project retains context over time.

## 9. Tests

- Keep automated tests in `tests/`.
- Use a simple in-project test framework rather than an external one.
- Put test data files in `tests/data/`.
- Copy test data into `bin/` when tests require runtime files there.
