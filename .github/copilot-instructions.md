# GitHub Copilot Instructions for `math`

These instructions guide AI coding agents working in this repository.

## Project overview
- Single C99 console program built with CMake, target executable is `math`.
- Core logic is in `src/main.c`; `CMakeLists.txt` just wires all `src/*.c` into the `math` executable.
- The program focuses on base-2–36 numeric representation and parsing, including repeating decimals, not on full arithmetic yet.

## Build & run workflow
- Configure (Ninja generator, Debug): `cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B build`.
- Build: `cmake --build build` (VS Code task: `build`).
- Run main binary (examples):
  - `./build/math` (runs built-in demo and parser tests).
  - `./build/math repl` (interactive REPL for entering numbers like `16#1A.3(45)`).
- Keep build artefacts in `build/`; do not add alternative build layouts without updating `CMakeLists.txt` and these instructions.

## Key types and invariants (`src/main.c`)
- `glyph_t`, `value_t`, `base_t` are `uint8_t` aliases; invalid sentinel value is always `255` (`INVALID_GLYPH`, `INVALID_VALUE`). Do not change these without auditing all comparisons.
- `MAX_EXT_DIGITS` defines the maximum digit value for bases up to 36 and is used to size digit buffers; any new logic must respect this limit.
- `number_proto_t` holds a base, a `digits` heap array, and its `length`; all digits are numeric values, not characters.
- `number_t` wraps `number_proto_t` and adds:
  - `is_negative` boolean flag (no sign stored in digits),
  - `decimal_length` (count of digits after the decimal point),
  - `repeating_length` (count of trailing digits that form the repeating block).
- `allocate_number_array` is the only allocator for `number_t` digits; always pair with `deallocate_number`.

## Parsing & normalization conventions
- Text-to-number API: `initialize_number_from_string(const char *str, base_t base)`.
  - Accepts optional leading `-`, one optional `.` decimal point, and at most one repeating section in parentheses, e.g. `"1.(3)"`, `"1A.3(45)"`.
  - Validation errors are reported via `stderr` and result in a `number_t` with `proto.digits == NULL` and `proto.length == 0`; callers must check for this before further use.
  - Repeating digits must be after a decimal point and non-empty; nested or multiple parentheses are rejected.
- `normalize_number` is called by the parser and is responsible for canonical representation:
  - Removes insignificant leading zeros before the decimal point.
  - Trims trailing zeros in the non-repeating decimal part only when there is no repeating block.
  - Preserves **all digits in a non-zero repeating section** exactly as provided, including internal zeros.
  - Collapses all-zero repeating sections (e.g. `"1.2(0)"`) into a non-repeating representation.
  - Normalizes `-0...` to `0` and clears `is_negative`.
- `display_number` is the canonical pretty-printer and should be kept consistent with `initialize_number_from_string` + `normalize_number` (e.g. base prefix `"%u#"`, decimal point, parentheses for repeating part).

## REPL and I/O patterns
- REPL entry point: `repl()` in `src/main.c`; it:
  - Accepts input as either `base#digits` (e.g. `"2#101.01"`, `"16#1A.3(45)")` or plain `digits` (defaults to base 10).
  - Uses `initialize_number_from_string` and then `display_number` to echo back the normalized form.
  - Recognizes `exit` to terminate; empty lines are ignored.
- When adding commands or operations to the REPL, prefer small, single-line commands (e.g. `op number` or `base#from -> base#to`) and reuse existing parsing/printing helpers.

## Error handling and testing style
- All parse/validation errors are currently printed using `fprintf(stderr, ...)` and the program continues; follow this pattern for new validations.
- The bottom of `main` contains a set of ad-hoc parser tests (valid and invalid cases) that run on every execution without `repl`:
  - If you add new features to the parser or normalizer, extend these test cases rather than introducing a separate framework right away.
  - Keep tests self-describing through `printf("Test N: ...")` labels, matching the existing style.

## Extending functionality
- When introducing new source files under `src/`, just drop in `*.c` and optionally `*.h`; `CMakeLists.txt` already uses `file(GLOB src/*.c)` so new modules are auto-linked into `math`.
- Put reusable helpers (e.g. base conversion, arithmetic on `number_t`) into separate translation units rather than further growing `main.c`, then add forward declarations in appropriate headers.
- Maintain the separation of concerns:
  - Parsing/normalization on the `number_t` structure.
  - I/O and user interaction in `main`/`repl`.
  - Future arithmetic or conversion operations should take and return `number_t` values rather than raw strings.
