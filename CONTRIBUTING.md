# Contributing to hunmin

Thank you for your interest in contributing to hunmin! This document explains how to
work with the repository, what kinds of contributions are welcome, and how to
propose changes.

## The one idea

**Layout behaviour is data, never code.** Adding a keyboard layout means adding a
JSON file and golden tests: no C, no engine rebuild. The engine knows how to
compose syllables and buffer an eojeol; it does not know what a keyboard is.

If your change requires a new C branch in the engine, it needs justification in
`docs/SPEC-P2.md`. The default answer is to express the behaviour with the
existing key primitives (`Literal | SlotJamo | Ambiguous | Sequence`) plus rule
tables and per-layout flags.

## What we accept

### Layout changes (the main contribution path)

New keyboard layouts go entirely in `libhunmin/data/layouts/` as JSON files.
See `docs/HOWTO-NEW-LAYOUT.md` for the step-by-step process.

A layout contribution consists of:
1. A JSON layout file under `libhunmin/data/layouts/`
2. Golden test vectors under `tests/golden/`
3. Running `tools/doctor.py libhunmin/data` to validate

No C code changes required.

### Engine behaviour changes

Changes to the core engine (`libhunmin/src/`) need:
- Justification in `docs/SPEC-P2.md` (what behaviour, why, what the alternatives were)
- Updated golden tests
- Passing `ctest` (all tiers)
- ABI compatibility (append-only — see `libhunmin/tests/abi/symbols.txt`)

### Frontend changes

Frontends stay thin (≈700 lines each). Logic that grows in a frontend belongs in
the core. The fcitx5 addon is in `frontends/fcitx5/`, the Android IME in
`frontends/android/`.

### Documentation

All documentation is in `docs/`. The `wiki/` directory holds session records and
decision history — it is not a second copy of `docs/`.

## How to propose a change

1. **Read the relevant docs first** — `AGENTS.md`, `docs/SPEC-P2.md`, `docs/HOWTO-NEW-LAYOUT.md`
2. **Make a worktree** — one task, one worktree off `master`
3. **Write the change** — keep commits small and verifiable
4. **Run the tests** — `ctest --test-dir build --output-on-failure`
5. **Run the linters** — `tools/doctor.py libhunmin/data`, `tools/docs-lint.py --build-dir build`
6. **Submit a PR** — the maintainer will review and merge

## Testing

All tests are CTest-based. Labels select tiers:

```bash
ctest --test-dir build -L tier-A          # engine unit tests (fast)
ctest --test-dir build -L tier-B          # fcitx5 addon tests
ctest --test-dir build -L tier-C          # trace conformance (requires -DHUNMIN_TRACE_BUILD=ON)
ctest --test-dir build -L tier-D          # mutation tests
ctest --test-dir build -L tier-E          # Android tests (requires emulator)
ctest --test-dir build -LE slow           # inner loop (everything cheap)
```

The `tools/ctest-select.sh` script prints what it will run and what it will not.

### Line endings

`.gitattributes` checks every text file out with LF (`text=auto eol=lf`), so a
Windows checkout with `core.autocrlf=true` still presents the byte-compared
inputs as LF: `libhunmin/tests/abi/symbols.txt` and
`tests/fixtures/krdict/expected/krdict.json`. Without it, `ctest`'s `abi` and
`krdict-fixture` guards report a change nobody made (issue #202). The
byte-faithful data directories (`data-orig/`, `libhunmin/data/`,
`tests/golden/`, `tests/conformance/`) are `-text` and keep their committed
bytes; `*.bat` is the one text type checked out CRLF. A new byte-compared text
input is covered by the default; a file that must keep CRLF needs an explicit
rule. An existing Windows checkout keeps its CRLF copies until they are checked
out again — `rm libhunmin/tests/abi/symbols.txt
tests/fixtures/krdict/expected/krdict.json && git checkout -- <those paths>`,
or a fresh clone, replaces them with LF.

## Licensing

hunmin is LGPL-2.1-or-later. Bundled data carries its own terms:
- `libhunmin/data/hanja/` — BSD-3 (from libhangul)
- `libhunmin/data/idioms/` — MIT
- `libhunmin/data/krdict/` — CC-BY-SA (from 국립국어원 한국어기초사전)

See `debian/copyright` for the full breakdown.

## Code style

- No invented abbreviations (see `docs/GLOSSARY.md`)
- Hanja only when it is the subject matter
- Write in English unless the content is specifically Korean
- Follow existing C/Kotlin conventions in the file you're editing

## Questions?

Open an issue on GitHub or check the `wiki/` for past decisions and discussions.