# Project guidance

Read these documents in order to understand the system, run it, learn how it
works, and then make changes according to its standards. Keep every
applicable document current in the same change as the system it describes;
update this index when documents move or new essential guidance is added.

- [Project overview](README.md): keep this short and limited to what the system
  does, prerequisites, and how to build, run and install it. Put technical
  details, architecture and implementation history in the appropriate `docs/`
  documents and link to them instead of expanding the README.
- [Change log](CHANGELOG.md): current user-facing additions, changes and fixes.
- [License](LICENSE): GPL-2.0-or-later for project-owned code; preserve the
  separately licensed components and historical assets identified in README.
- [Hosted development guide](docs/guides/HOSTED_DEVELOPMENT.md): Rasta setup,
  build options, test execution and VS Code debugging.
- [Independent samples](docs/guides/SAMPLES.md): application layout, private
  dependencies and standalone builds against the public GEM SDK.
- [Linux backend guide](docs/guides/GEMIX_LINUX.md): native backend configuration,
  packaging and deployment.
- [Linux paths and installation](docs/guides/LINUX_PATHS.md): standard install
  layout, XDG storage, Docker mounts and legacy settings migration.
- [Packaging contract](docs/guides/PACKAGING.md): minimal runtime/SDK contents,
  dependency notices, staging and integrity checks.
- [Configuration guide](docs/guides/CONFIGURATION.md): XDG component files,
  syntax, Desktop locations and the libgemconf API.
- [Gemscape browser](docs/guides/GEMSCAPE.md): input, settings, profiles,
  cookies/cache, acceptance coverage and embedded-browser limits.
- [API transport](docs/architecture/API_TRANSPORT.md): libgem/gemd architecture,
  protocol, API coverage and compatibility limits.
- [Security and safety](docs/architecture/SECURITY.md): trust boundaries,
  validation, ownership and deployment constraints.
- [Security audit](docs/tests/SECURITY_AUDIT.md): coverage, findings, regression
  evidence and remaining analyzer diagnostics; refresh after security changes.
- [UAT catalog](docs/tests/UAT.md): demo behavior, acceptance scenarios and
  reference-image review.
- [Test reporting](docs/tests/README.md): report generation and evidence storage.
- [Latest test report](docs/tests/LATEST.md): results of the latest full run;
  regenerate through `make tests` when validating implementation changes.
- [Implementation notes](docs/notes/README.md): current compatibility worklists
  and clearly identified historical context.
- [Proposed work](docs/notes/TODO.md): remaining compatibility, security and
  performance work; revise as items are completed or priorities change.
- [Directory standard](docs/standards/DIRECTORIES_STANDARD.md): mandatory
  source, build, test and documentation organization.
- [C standard](docs/standards/C_STANDARD.md): mandatory coding, documentation,
  compiler and verification rules.
- [Original manuals](docs/manuals/README.md): historical specifications and
  provenance; preserve originals and record port-specific findings in notes.
