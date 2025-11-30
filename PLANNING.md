# Math Project Planning

This document captures the current design direction and plans for `number_t` and arithmetic.

## Current Representation

- Core numeric type: `number_t` in `src/main.c`.
- Fields:
  - `proto.base` (`base_t`): base 2–36.
  - `proto.digits` (`value_t *`): dynamic big-digit array, most-significant digit first.
  - `proto.length` (`size_t`): number of digits.
  - `is_negative` (`bool`): sign (no sign stored in digits).
  - `decimal_length` (`size_t`): count of digits after the radix point.
  - `repeating_length` (`size_t`): count of trailing digits that form the repeating block.
- Invariants:
  - `INVALID_GLYPH` / `INVALID_VALUE` are always `255` (`uint8_t`).
  - `MAX_EXT_DIGITS` is sized for bases up to 36 and used only for fixed buffers / glyph mapping.
  - `normalize_number` is the canonicalizer and must be kept consistent with parsing and printing.

## Big-Int Goal

- Treat `number_t` itself as the arbitrary-precision number type:
  - `proto.digits` is the big-int coefficient vector in base `proto.base`.
  - `decimal_length` and `repeating_length` encode exact radix point position and periodic tail.
  - No separate big-int struct; all arithmetic is expressed in terms of `number_t` and its digit arrays.
- Long-term: support full algebra on `number_t` (add, sub, mul, div, comparison) with exact rational semantics in any base.

## Current Addition Pipeline (Interim)

- `number_add(const number_t *a, const number_t *b)` (same base required) currently:
  - Converts each `number_t` to an intermediate `rational_t` with `uint64_t` numerator/denominator.
  - Adds rationals via `rational_add`.
  - Converts result back to a `number_t` using `rational_to_number`.
- Limitations of this interim pipeline:
  - Uses `uint64_t` / `int64_t`: overflow for very large digit counts.
  - `rational_to_number` emits a finite expansion up to a fixed digit cap and always sets `repeating_length = 0`.
  - This is a temporary step toward a pure `number_t` big-int implementation.

## Target Design for Exact, Scalable Arithmetic

### Role of `number_t`

- `number_t` is the single source of truth for values:
  - Arbitrary-precision via heap-allocated `proto.digits`.
  - Exact rational encoding via `(decimal_length, repeating_length)` over the base-`proto.base` digits.

### Planned Pure-`number_t` Addition

1. **Scale Alignment**
   - Ensure both operands share a common decimal scale:
     - Pad the shorter `decimal_length` side with trailing zeros in its digit array.
   - Align repeating patterns:
     - Interpret `repeating_length == 0` as repeating zeros.
     - Compute a common repeat period `L = lcm(rlen_a, rlen_b)` (with `0` treated as period `1`).

2. **Big-Digit Operations**
   - Implement helpers operating directly on `value_t` arrays:
     - `digits_add(...)` for arbitrary-precision addition in base `proto.base`.
     - `digits_sub_abs(...)` for magnitude subtraction.
     - `number_compare_abs(const number_t *a, const number_t *b)` to drive sign handling.
   - Addition logic:
     - Align integer and fractional parts, conceptually expand repeating sections up to a shared window, then call `digits_add`.

3. **Repetition Detection in Result**
   - After digit-wise addition:
     - Scan fractional digits to find a minimal pair `(prefix_len, period_len)` where the tail is periodic.
     - Encode:
       - `decimal_length = prefix_len + period_len`.
       - `repeating_length = period_len` (0 if purely terminating or all-zero tail).
     - Call `normalize_number` to apply existing canonicalization (strip insignificant zeros, normalize `-0`).

4. **API Surface**
   - Keep `number_add` as the public addition operation.
   - Internally:
     - Use sign-aware composition over `number_compare_abs` and `digits_add` / `digits_sub_abs`.
     - Remove the interim `rational_t` layer once the pure `number_t` implementation is stable.

## Near-Term Implementation Steps

1. Introduce internal big-digit helpers (all in terms of `value_t` and `base_t`).
2. Re-implement `number_add` to use these helpers directly on `number_t` (terminating decimals only at first). **This is done.**
3. Extend fractional handling to align and add non-repeating decimals exactly. **This is done.**
4. Add repeating-decimal support in `number_add` using a rational-style pipeline expressed entirely in terms of `number_t`:
   - For each operand, decompose digits into integer `I`, non-repeating fractional `F`, and repeating block `R` in base `b = proto.base`.
   - Build exact integer numerator/denominator pairs `(num, den)` as integer-only `number_t` values using digit algebra and the standard formulas:
     - Non-repeating: `value = (I·b^|F| + F) / b^|F|`.
     - Repeating: `value = (C - B) / (b^|F| · (b^|R| - 1))` where `B` is digits of `I`+`F` and `C` is digits of `I`+`F`+`R`.
   - Implement integer-only helpers on `number_t` (with `decimal_length = 0`, `repeating_length = 0`):
     - `number_int_add_abs` (big-digit add),
     - `number_int_sub_abs` (big-digit sub),
     - `number_int_mul_abs` (schoolbook mul),
     - `number_int_divmod_abs` (long division, quotient + remainder).
   - For `number_add(a, b)`:
     - Convert each to `(num_a, den_a)` and `(num_b, den_b)` as integer `number_t`s.
     - Compute `num = num_a * den_b + num_b * den_a` and `den = den_a * den_b` using the integer helpers.
     - (Optionally) reduce `num/den` via a `number_int_gcd` helper when available.
     - Convert `(num, den)` back to `number_t` in base `b` by:
       - Doing integer division `num / den` to get the integer part.
       - Using repeated `remainder *= b; digit = remainder / den; remainder %= den;` to generate fractional digits.
       - Tracking remainders and their first positions to detect the start and length of the repeating cycle.
       - Setting `decimal_length` and `repeating_length` from the detected prefix and period, then calling `normalize_number`.
5. Delete any remaining references to non-`number_t` numeric types (e.g., old `rational_t`) once the pure `number_t` pipeline is fully in place and REPL tests pass for repeating and non-repeating cases.

This file is the reference for future arithmetic work: new operations should respect `number_t`’s role as the core, arbitrary-precision, base-2–36 rational representation with optional repeating sections.
