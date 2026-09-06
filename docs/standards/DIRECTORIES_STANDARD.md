# Directory standard

Keep source, documentation, test data and generated outputs in the locations
below. Do not create files outside the approved project directories or leave
editor backups, stray object files or temporary files in the tree.

## Project layout

- `src/`: project implementation `.c` files.
- `samples/`: independent applications with `src/`, `include/`, `lib/` and `data/`.
  Integrated executables go in `bin/samples/`, private libraries in
  `bin/samples/lib/`, and resources in `bin/samples/data/`. Only build entry points belong at the samples root.
- `include/`: GEM headers only. Sample headers belong in `samples/include/`;
  test-only headers belong in `tests/include/`.
  Shared transport declarations belong in `include/gem/gemd.h`.
- `lib/`: GEM implementation dependencies and platform backends only.
  Sample-specific and third-party application libraries go in `samples/lib/`.
- `bin/`: final programs, final generated outputs and runtime resources.
- `build/`: all intermediate build artifacts, including objects, dependency
  files, static libraries, maps, listings and temporary build products.
  Detailed test logs and captured framebuffers also belong here. Pinned
  dependency archives and extracted sources belong under the selected build
  directory in `deps/`, never in a nested Git repository in the source tree.
- `docs/`: documentation, organized into the subdirectories below.
- `tests/`: automated tests, organized by scope as described below.
- `tools/`: utility subdirectories only; no files directly at this level.
  Shell and CMake scripts live in `tools/scripts/`; compiled resource
  generators live in `tools/resgen/`; container recipes live in `tools/container/`.
- `.vscode/`: editor build, launch and debugging configuration.

Public GEM headers must live in `include/`. Project implementation source may live
in `src/` or a library directory under `lib/`; sample application source
belongs in `samples/src/` or `samples/lib/`. Automated test source belongs
in `tests/`, and development utility source belongs in `tools/`.
Use lowercase snake_case for C source and header file names.

Only normal top-level project metadata, such as `LICENSE`, `README.md`,
`Makefile`, `CMakeLists.txt` and `AGENTS.md`, belongs at the project root.

## Build organization

- Put intermediate build outputs only under the root `build/` directory.
  Final programs and runtime resources go in `bin/`.
- Do not create additional `build/` directories inside `src/`, `lib/`,
  `tests/` or other subdirectories.
- Use nested makefiles. The top-level `Makefile` defines the main entry points;
  subdirectories such as `src/`, `lib/`, `lib/dbf/` and `tests/` should have
  their own makefiles when needed.
- `make` builds the project, including tests; `make tests` runs all suites
  and generates the test report.

## Tests and data

- `tests/unit/`: focused unit tests.
- `tests/integration/`: tests of cooperating components and real-server RPC.
- `tests/uat/`: user acceptance demos and automated interaction scenarios.
  Demos use the `demoN` naming pattern.
- `tests/include/`: headers belonging only to tests and fixture generators.
- `tests/data/`: test input data and reviewed reference images.
- Copy test data into `bin/` when tests require runtime files there.
- Generated evidence belongs under root `build/`; execution reports belong
  in `docs/tests/`.

## Documentation

Do not place files directly in `docs/`. Use a meaningful subdirectory:

- `docs/standards/`: coding and repository organization rules.
- `docs/architecture/`: system design, API transport and security boundaries.
- `docs/guides/`: build, usage and deployment guides.
- `docs/manuals/`: original reference manuals.
- `docs/notes/`: project notes, implementation details and gap analyses.
- `docs/images/originals/`: original visual reference assets.
- `docs/images/screenshots/`: captures of running GEM sessions used in documentation.
- `docs/tests/`: test scenarios, reporting instructions and execution reports.

Keep `README.md` short: system purpose, prerequisites, build, run and install
instructions. Put technical details in the appropriate `docs/` documents and
link to them from the README when needed.
Retain project context in `docs/notes/`. Update affected documentation and
links whenever implementation, behavior or file locations change. Update
report generators when report format or links change; generate execution
reports from actual test runs. Preserve original manuals, upstream vendor
documentation and dated execution outcomes; distinguish them from current
project guidance. See the [notes index](../notes/README.md) and
[manual catalog](../manuals/README.md).

See the [C standard](C_STANDARD.md) for module layout and coding rules.
