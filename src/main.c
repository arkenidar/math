// plan to use C99

#include <stdio.h>
#include <string.h> // For strlen, strcmp, sscanf

// Maximum number of extended digits (0-9, A-Z)
// Calculation: 1 (for '0') + 9 (for '1'-'9') + 1 (for 'A') + 25 (for 'A'-'Z') =
// 36 Thus, MAX_EXT_DIGITS = 36 Simplified calculation: '0' to '9' = 10 digits
// 'A' to 'Z' = 26 digits
// Total = 10 + 26 = 36
// Therefore:
// MAX_EXT_DIGITS = 36
// Useful for bases up to 36 (0-9, A-Z)
#define MAX_EXT_DIGITS (1 + ('9' - '0') + 1 + ('Z' - 'A'))

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

#include <stdbool.h> // For bool type
#include <stddef.h>  // For size_t
#include <stdint.h>  // For uint8_t
#include <stdlib.h>  // For malloc, free

typedef uint8_t glyph_t;
#define INVALID_GLYPH 255

typedef uint8_t value_t;
#define INVALID_VALUE 255

typedef uint8_t base_t;

typedef struct {
  base_t base;     // Base of the number system
  value_t *digits; // Pointer to an array of digits (values)
  size_t length;   // Number of digits
                   // Additional fields can be added as needed
  // e.g., negative sign, decimal point position, repeating glyphs/decimals,
  // etc. For simplicity, these are omitted in this basic structure
} number_proto_t;

typedef struct {
  number_proto_t proto; // Prototype of the number
  // Additional fields for managing the number can be added here
  // e.g., memory management, flags, etc.

  // negative sign flag
  bool is_negative;

  // decimal part length
  size_t decimal_length;

  // repeating decimals length
  size_t repeating_length;
} number_t;

// Forward declarations
glyph_t value_to_glyph(value_t value);
value_t glyph_to_value(glyph_t glyph);

// Minimal rational layer built on top of integer-only number_t values.
// Invariants:
// - num and den share the same base
// - den represents a strictly positive integer (no sign, no decimals)
// - num may be negative; sign is always carried by num
// - Both num and den are kept in normalized, integer-only form
typedef struct {
  number_t num; // numerator
  number_t den; // denominator (always > 0)
} rational_t;

// Forward declarations that depend on number_t
number_t initialize_number_from_string(const char *str, base_t base);
void normalize_number(number_t *num);
number_t number_add(const number_t *a, const number_t *b);

// Integer-only helpers on number_t (decimal_length == 0, repeating_length == 0)
number_t number_int_add_abs(const number_t *a, const number_t *b);
number_t number_int_sub_abs(const number_t *a, const number_t *b,
                            bool negative_result);
number_t number_int_mul_abs(const number_t *a, const number_t *b);
void number_int_divmod_abs(const number_t *numerator,
                           const number_t *denominator, number_t *quotient,
                           number_t *remainder);
number_t number_int_gcd_abs(const number_t *a, const number_t *b);

// Rational helpers
rational_t rational_make_from_ints(const number_t *num, const number_t *den);
void rational_deallocate(rational_t *r);
void rational_normalize(rational_t *r);
rational_t rational_add(const rational_t *a, const rational_t *b);
rational_t rational_from_terminating_number(const number_t *x);
rational_t rational_from_repeating_number(const number_t *x);

// NOTE: All arithmetic is intended to be expressed directly in terms of
// number_t and its digit arrays. Any previous rational_t / uint64_t helpers
// have been removed in favor of pure big-digit operations.

// Helper: deallocate and reset number to empty/NaN-like state
void reset_number(number_t *num) {
  if (num == NULL) {
    return;
  }
  if (num->proto.digits != NULL) {
    free(num->proto.digits);
  }
  num->proto.digits = NULL;
  num->proto.length = 0;
  num->proto.base = 0;
  num->is_negative = false;
  num->decimal_length = 0;
  num->repeating_length = 0;
}

// allocate number structure
// Returns a number_t with digits set to NULL on allocation failure
number_t allocate_number_array(base_t base, size_t length) {
  number_t num;
  num.proto.base = base;
  num.proto.length = length;
  num.proto.digits = (value_t *)malloc(length * sizeof(value_t));

  // Check for allocation failure
  if (num.proto.digits == NULL) {
    fprintf(stderr, "Error: Failed to allocate memory for %zu digits\n",
            length);
    num.proto.length = 0;
    num.is_negative = false;
    num.decimal_length = 0;
    num.repeating_length = 0;
    return num;
  }

  num.is_negative = false;
  num.decimal_length = 0;
  num.repeating_length = 0;
  return num;
}

// number deallocation function
void deallocate_number(number_t *num) {
  if (num->proto.digits != NULL) {
    free(num->proto.digits);
    num->proto.digits = NULL;
  }
}

// Integer-only absolute addition: assumes both a and b are non-negative
// integers in the same base with decimal_length == 0 and
// repeating_length == 0. Result is a non-negative integer number_t.
number_t number_int_add_abs(const number_t *a, const number_t *b) {
  number_t result = allocate_number_array(a ? a->proto.base : 10, 0);

  if (!a || !b || !a->proto.digits || !b->proto.digits ||
      a->proto.base != b->proto.base) {
    fprintf(stderr, "Error: number_int_add_abs requires two valid integers in "
                    "the same base.\n");
    return result;
  }

  if (a->decimal_length != 0 || b->decimal_length != 0 ||
      a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(stderr, "Error: number_int_add_abs expects integer operands.\n");
    return result;
  }

  base_t base = a->proto.base;
  size_t max_len =
      (a->proto.length > b->proto.length) ? a->proto.length : b->proto.length;

  result = allocate_number_array(base, max_len + 1);
  if (!result.proto.digits) {
    return result;
  }

  long carry = 0;
  size_t ri = max_len + 1;

  for (size_t k = 0; k < max_len; k++) {
    size_t a_pos =
        (a->proto.length > k) ? (a->proto.length - 1 - k) : (size_t)-1;
    size_t b_pos =
        (b->proto.length > k) ? (b->proto.length - 1 - k) : (size_t)-1;
    long da = (a->proto.length > k && a_pos < a->proto.length)
                  ? (long)a->proto.digits[a_pos]
                  : 0;
    long db = (b->proto.length > k && b_pos < b->proto.length)
                  ? (long)b->proto.digits[b_pos]
                  : 0;
    long sum = da + db + carry;
    carry = sum / base;
    long digit = sum % base;
    if (digit < 0) {
      digit += base;
      carry--;
    }
    ri--;
    result.proto.digits[ri] = (value_t)digit;
  }

  if (carry != 0) {
    ri--;
    result.proto.digits[ri] = (value_t)carry;
  }

  size_t used = (max_len + 1) - ri;
  for (size_t i = 0; i < used; i++) {
    result.proto.digits[i] = result.proto.digits[ri + i];
  }

  result.proto.length = used;
  result.is_negative = false;
  result.decimal_length = 0;
  result.repeating_length = 0;

  normalize_number(&result);
  return result;
}

// Integer-only absolute subtraction: computes |a| - |b| where |a| >= |b|,
// with all values treated as non-negative integers. The negative_result flag
// allows the caller to control the sign on the resulting number_t.
number_t number_int_sub_abs(const number_t *a, const number_t *b,
                            bool negative_result) {
  number_t result = allocate_number_array(a ? a->proto.base : 10, 0);

  if (!a || !b || !a->proto.digits || !b->proto.digits ||
      a->proto.base != b->proto.base) {
    fprintf(stderr, "Error: number_int_sub_abs requires two valid integers in "
                    "the same base.\n");
    return result;
  }

  if (a->decimal_length != 0 || b->decimal_length != 0 ||
      a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(stderr, "Error: number_int_sub_abs expects integer operands.\n");
    return result;
  }

  base_t base = a->proto.base;
  size_t max_len =
      (a->proto.length > b->proto.length) ? a->proto.length : b->proto.length;

  result = allocate_number_array(base, max_len);
  if (!result.proto.digits) {
    return result;
  }

  long borrow = 0;
  size_t ri = max_len;

  for (size_t k = 0; k < max_len; k++) {
    size_t a_pos =
        (a->proto.length > k) ? (a->proto.length - 1 - k) : (size_t)-1;
    size_t b_pos =
        (b->proto.length > k) ? (b->proto.length - 1 - k) : (size_t)-1;
    long da = (a->proto.length > k && a_pos < a->proto.length)
                  ? (long)a->proto.digits[a_pos]
                  : 0;
    long db = (b->proto.length > k && b_pos < b->proto.length)
                  ? (long)b->proto.digits[b_pos]
                  : 0;
    long diff = da - db - borrow;
    if (diff < 0) {
      diff += base;
      borrow = 1;
    } else {
      borrow = 0;
    }
    ri--;
    result.proto.digits[ri] = (value_t)diff;
  }

  size_t used = max_len - ri;
  for (size_t i = 0; i < used; i++) {
    result.proto.digits[i] = result.proto.digits[ri + i];
  }

  result.proto.length = used;
  result.is_negative = negative_result;
  result.decimal_length = 0;
  result.repeating_length = 0;

  normalize_number(&result);
  return result;
}

// Integer-only absolute multiplication: |a| * |b| using schoolbook
// multiplication. Both operands must be non-negative integers in the same
// base with decimal_length == 0 and repeating_length == 0.
number_t number_int_mul_abs(const number_t *a, const number_t *b) {
  number_t result = allocate_number_array(a ? a->proto.base : 10, 0);

  if (!a || !b || !a->proto.digits || !b->proto.digits ||
      a->proto.base != b->proto.base) {
    fprintf(stderr, "Error: number_int_mul_abs requires two valid integers in "
                    "the same base.\n");
    return result;
  }

  if (a->decimal_length != 0 || b->decimal_length != 0 ||
      a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(stderr, "Error: number_int_mul_abs expects integer operands.\n");
    return result;
  }

  base_t base = a->proto.base;

  // If either operand is zero, return zero.
  bool a_is_zero = true;
  for (size_t i = 0; i < a->proto.length; i++) {
    if (a->proto.digits[i] != 0) {
      a_is_zero = false;
      break;
    }
  }
  bool b_is_zero = true;
  for (size_t i = 0; i < b->proto.length; i++) {
    if (b->proto.digits[i] != 0) {
      b_is_zero = false;
      break;
    }
  }

  if (a_is_zero || b_is_zero) {
    result = allocate_number_array(base, 1);
    if (result.proto.digits) {
      result.proto.digits[0] = 0;
      result.is_negative = false;
      result.decimal_length = 0;
      result.repeating_length = 0;
    }
    return result;
  }

  size_t len_a = a->proto.length;
  size_t len_b = b->proto.length;
  size_t len_res = len_a + len_b;

  result = allocate_number_array(base, len_res);
  if (!result.proto.digits) {
    return result;
  }

  // Initialize to zero.
  for (size_t i = 0; i < len_res; i++) {
    result.proto.digits[i] = 0;
  }

  // Schoolbook multiplication from least significant digits.
  for (size_t ia = 0; ia < len_a; ia++) {
    size_t pos_a = len_a - 1 - ia;
    long da = (long)a->proto.digits[pos_a];
    long carry = 0;

    for (size_t ib = 0; ib < len_b; ib++) {
      size_t pos_b = len_b - 1 - ib;
      size_t pos_res = len_res - 1 - (ia + ib);
      long db = (long)b->proto.digits[pos_b];
      long existing = (long)result.proto.digits[pos_res];
      long prod = da * db + existing + carry;
      result.proto.digits[pos_res] = (value_t)(prod % base);
      carry = prod / base;
    }

    // Propagate any remaining carry.
    size_t pos_res = len_res - 1 - (ia + len_b);
    while (carry > 0 && pos_res < len_res) {
      long existing = (long)result.proto.digits[pos_res];
      long sum = existing + carry;
      result.proto.digits[pos_res] = (value_t)(sum % base);
      carry = sum / base;
      if (pos_res == 0) {
        break;
      }
      pos_res--;
    }
  }

  result.is_negative = false;
  result.decimal_length = 0;
  result.repeating_length = 0;

  normalize_number(&result);
  return result;
}

// Internal helper: compare absolute values of two integer-only numbers
// (no decimals, no repeating). Returns -1, 0, or 1.
static int compare_int_abs(const number_t *a, const number_t *b) {
  if (!a || !b || !a->proto.digits || !b->proto.digits) {
    return 0;
  }

  if (a->proto.length < b->proto.length) {
    return -1;
  } else if (a->proto.length > b->proto.length) {
    return 1;
  }

  for (size_t i = 0; i < a->proto.length; i++) {
    value_t da = a->proto.digits[i];
    value_t db = b->proto.digits[i];
    if (da < db) {
      return -1;
    } else if (da > db) {
      return 1;
    }
  }
  return 0;
}

// Internal helper: integer-only scalar multiplication of a non-negative
// integer 'src' by a single digit factor in [0, base). Result is stored
// into 'dst', which must already be allocated with at least src.length+1
// digits and same base.
static void int_scalar_mul(const number_t *src, value_t factor, number_t *dst) {
  base_t base = src->proto.base;
  size_t len = src->proto.length;

  // Initialize dst as zero of appropriate length.
  dst->proto.base = base;
  dst->proto.length = len + 1;
  for (size_t i = 0; i < dst->proto.length; i++) {
    dst->proto.digits[i] = 0;
  }

  long carry = 0;
  for (size_t k = 0; k < len; k++) {
    size_t pos = len - 1 - k;
    long prod = (long)src->proto.digits[pos] * (long)factor + carry;
    dst->proto.digits[pos + 1] = (value_t)(prod % base);
    carry = prod / base;
  }
  dst->proto.digits[0] = (value_t)carry;

  dst->is_negative = false;
  dst->decimal_length = 0;
  dst->repeating_length = 0;
  normalize_number(dst);
}

// Integer-only absolute division with remainder: computes
//   quotient = floor(|numerator| / |denominator|)
//   remainder = |numerator| - quotient * |denominator|
// Both inputs must be non-negative integers in the same base with
// decimal_length == 0 and repeating_length == 0.
// The outputs are allocated via allocate_number_array and must be
// deallocated by the caller using deallocate_number.
void number_int_divmod_abs(const number_t *numerator,
                           const number_t *denominator, number_t *quotient,
                           number_t *remainder) {
  if (!quotient || !remainder) {
    fprintf(
        stderr,
        "Error: number_int_divmod_abs requires non-null output pointers.\n");
    return;
  }

  // Initialize outputs to empty numbers in base 10 by default.
  *quotient = allocate_number_array(10, 0);
  *remainder = allocate_number_array(10, 0);

  if (!numerator || !denominator || !numerator->proto.digits ||
      !denominator->proto.digits ||
      numerator->proto.base != denominator->proto.base) {
    fprintf(stderr, "Error: number_int_divmod_abs requires two valid integers "
                    "in the same base.\n");
    return;
  }

  if (numerator->decimal_length != 0 || denominator->decimal_length != 0 ||
      numerator->repeating_length != 0 || denominator->repeating_length != 0) {
    fprintf(stderr, "Error: number_int_divmod_abs expects integer operands.\n");
    return;
  }

  base_t base = numerator->proto.base;

  // Check for zero denominator.
  bool denom_is_zero = true;
  for (size_t i = 0; i < denominator->proto.length; i++) {
    if (denominator->proto.digits[i] != 0) {
      denom_is_zero = false;
      break;
    }
  }
  if (denom_is_zero) {
    fprintf(stderr, "Error: number_int_divmod_abs division by zero.\n");
    return;
  }

  // Quick check: if numerator < denominator, quotient = 0, remainder =
  // numerator.
  int cmp0 = compare_int_abs(numerator, denominator);
  if (cmp0 < 0) {
    *quotient = allocate_number_array(base, 1);
    if (quotient->proto.digits) {
      quotient->proto.digits[0] = 0;
      quotient->is_negative = false;
      quotient->decimal_length = 0;
      quotient->repeating_length = 0;
    }
    *remainder = allocate_number_array(base, numerator->proto.length);
    if (remainder->proto.digits) {
      for (size_t i = 0; i < numerator->proto.length; i++) {
        remainder->proto.digits[i] = numerator->proto.digits[i];
      }
      remainder->proto.length = numerator->proto.length;
      remainder->is_negative = false;
      remainder->decimal_length = 0;
      remainder->repeating_length = 0;
      normalize_number(remainder);
    }
    return;
  }

  // Allocate quotient and remainder with proper base.
  *quotient = allocate_number_array(base, numerator->proto.length);
  *remainder = allocate_number_array(base, numerator->proto.length + 1);
  if (!quotient->proto.digits || !remainder->proto.digits) {
    return;
  }

  // Initialize quotient digits to zero.
  for (size_t i = 0; i < quotient->proto.length; i++) {
    quotient->proto.digits[i] = 0;
  }

  // Initialize remainder to zero.
  remainder->proto.length = 1;
  remainder->proto.digits[0] = 0;
  remainder->is_negative = false;
  remainder->decimal_length = 0;
  remainder->repeating_length = 0;

  size_t n_len = numerator->proto.length;

  // Long division from most significant digit to least (MSB-first).
  for (size_t i = 0; i < n_len; i++) {
    // remainder = remainder * base + current_digit
    long carry = numerator->proto.digits[i];
    for (size_t k = 0; k < remainder->proto.length; k++) {
      size_t pos = remainder->proto.length - 1 - k;
      long val = (long)remainder->proto.digits[pos] * (long)base + carry;
      remainder->proto.digits[pos] = (value_t)(val % base);
      carry = val / base;
    }
    if (carry > 0) {
      // Shift digits right by one to accommodate extra carry.
      for (size_t k = remainder->proto.length; k > 0; k--) {
        remainder->proto.digits[k] = remainder->proto.digits[k - 1];
      }
      remainder->proto.digits[0] = (value_t)carry;
      remainder->proto.length += 1;
    }
    normalize_number(remainder);

    // Binary search for largest q_digit in [0, base-1] such that
    // denominator * q_digit <= remainder.
    value_t q_digit = 0;
    value_t low = 0;
    value_t high = base - 1;

    // Temporary product holder for denom * q_candidate.
    number_t prod = allocate_number_array(base, denominator->proto.length + 1);
    if (!prod.proto.digits) {
      return;
    }

    while (low <= high) {
      value_t mid = (value_t)((low + high) / 2);
      int_scalar_mul(denominator, mid, &prod);
      int cmp = compare_int_abs(&prod, remainder);
      if (cmp <= 0) {
        q_digit = mid;
        if (mid == base - 1) {
          break;
        }
        low = mid + 1;
      } else {
        if (mid == 0) {
          break;
        }
        high = mid - 1;
      }
    }

    deallocate_number(&prod);

    // Store quotient digit.
    quotient->proto.digits[i] = q_digit;

    // remainder = remainder - denominator * q_digit
    if (q_digit != 0) {
      number_t dq = allocate_number_array(base, denominator->proto.length + 1);
      if (!dq.proto.digits) {
        return;
      }
      int_scalar_mul(denominator, q_digit, &dq);
      number_t tmp = number_int_sub_abs(remainder, &dq, false);
      deallocate_number(remainder);
      *remainder = tmp;
      deallocate_number(&dq);
    }
  }

  // Final normalization of quotient and remainder.
  quotient->is_negative = false;
  quotient->decimal_length = 0;
  quotient->repeating_length = 0;
  normalize_number(quotient);

  remainder->is_negative = false;
  remainder->decimal_length = 0;
  remainder->repeating_length = 0;
  normalize_number(remainder);
}

// Integer-only greatest common divisor: returns gcd(|a|, |b|) as a
// non-negative integer in the same base, using Euclid's algorithm and
// number_int_divmod_abs. Both a and b must be non-negative integers with
// decimal_length == 0 and repeating_length == 0.
number_t number_int_gcd_abs(const number_t *a, const number_t *b) {
  number_t zero = allocate_number_array(a ? a->proto.base : 10, 1);
  if (!zero.proto.digits) {
    return zero;
  }
  zero.proto.digits[0] = 0;
  zero.is_negative = false;
  zero.decimal_length = 0;
  zero.repeating_length = 0;

  if (!a || !b || !a->proto.digits || !b->proto.digits ||
      a->proto.base != b->proto.base) {
    fprintf(stderr,
            "Error: number_int_gcd_abs requires two valid integers in the "
            "same base.\n");
    return zero;
  }

  if (a->decimal_length != 0 || b->decimal_length != 0 ||
      a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(stderr, "Error: number_int_gcd_abs expects integer operands.\n");
    return zero;
  }

  base_t base = a->proto.base;

  // Make local copies to avoid mutating inputs.
  number_t x = allocate_number_array(base, a->proto.length);
  number_t y = allocate_number_array(base, b->proto.length);
  if (!x.proto.digits || !y.proto.digits) {
    deallocate_number(&x);
    deallocate_number(&y);
    return zero;
  }
  for (size_t i = 0; i < a->proto.length; i++) {
    x.proto.digits[i] = a->proto.digits[i];
  }
  x.proto.length = a->proto.length;
  x.is_negative = false;
  x.decimal_length = 0;
  x.repeating_length = 0;
  normalize_number(&x);

  for (size_t i = 0; i < b->proto.length; i++) {
    y.proto.digits[i] = b->proto.digits[i];
  }
  y.proto.length = b->proto.length;
  y.is_negative = false;
  y.decimal_length = 0;
  y.repeating_length = 0;
  normalize_number(&y);

  // Euclid's algorithm: gcd(x, y) with x, y >= 0.
  while (!(y.proto.length == 1 && y.proto.digits[0] == 0)) {
    number_t q, r;
    number_int_divmod_abs(&x, &y, &q, &r);
    deallocate_number(&q);
    deallocate_number(&x);
    x = y;
    y = r;
  }

  // x now holds the gcd.
  deallocate_number(&y);
  deallocate_number(&zero);
  return x;
}

//======================= RATIONAL HELPERS =======================//

// Construct a rational from two integer-only numbers. The sign is moved
// entirely onto the numerator; the denominator is made positive. Both
// components are copied so the caller can free the originals.
rational_t rational_make_from_ints(const number_t *num, const number_t *den) {
  rational_t r;

  // Initialize to a safe default 0/1 in base 10 in case of errors.
  r.num = allocate_number_array(10, 1);
  r.den = allocate_number_array(10, 1);
  if (r.num.proto.digits) {
    r.num.proto.digits[0] = 0;
    r.num.is_negative = false;
    r.num.decimal_length = 0;
    r.num.repeating_length = 0;
  }
  if (r.den.proto.digits) {
    r.den.proto.digits[0] = 1;
    r.den.is_negative = false;
    r.den.decimal_length = 0;
    r.den.repeating_length = 0;
  }

  if (!num || !den || !num->proto.digits || !den->proto.digits ||
      num->proto.base != den->proto.base) {
    fprintf(stderr,
            "Error: rational_make_from_ints requires same-base integers.\n");
    return r;
  }

  if (num->decimal_length != 0 || den->decimal_length != 0 ||
      num->repeating_length != 0 || den->repeating_length != 0) {
    fprintf(stderr,
            "Error: rational_make_from_ints expects integer-only operands.\n");
    return r;
  }

  // Ensure denominator is not zero.
  bool den_is_zero = true;
  for (size_t i = 0; i < den->proto.length; i++) {
    if (den->proto.digits[i] != 0) {
      den_is_zero = false;
      break;
    }
  }
  if (den_is_zero) {
    fprintf(stderr, "Error: rational_make_from_ints denominator is zero.\n");
    return r;
  }

  base_t base = num->proto.base;

  // Copy numerator.
  deallocate_number(&r.num);
  r.num = allocate_number_array(base, num->proto.length);
  if (!r.num.proto.digits) {
    return r;
  }
  for (size_t i = 0; i < num->proto.length; i++) {
    r.num.proto.digits[i] = num->proto.digits[i];
  }
  r.num.proto.length = num->proto.length;
  r.num.is_negative = num->is_negative;
  r.num.decimal_length = 0;
  r.num.repeating_length = 0;
  normalize_number(&r.num);

  // Copy denominator, strip sign (denominator always positive).
  deallocate_number(&r.den);
  r.den = allocate_number_array(base, den->proto.length);
  if (!r.den.proto.digits) {
    return r;
  }
  for (size_t i = 0; i < den->proto.length; i++) {
    r.den.proto.digits[i] = den->proto.digits[i];
  }
  r.den.proto.length = den->proto.length;
  r.den.is_negative = false;
  r.den.decimal_length = 0;
  r.den.repeating_length = 0;
  normalize_number(&r.den);

  // Move any negative sign from the denominator onto the numerator.
  // (Currently initialize_number_from_string never sets den negative,
  // but this keeps the invariant explicit.)
  if (den->is_negative) {
    r.num.is_negative = !r.num.is_negative;
  }

  return r;
}

void rational_deallocate(rational_t *r) {
  if (!r) {
    return;
  }
  deallocate_number(&r->num);
  deallocate_number(&r->den);
}

// Normalize a rational by dividing numerator and denominator by their gcd,
// and ensuring the denominator is strictly positive.
void rational_normalize(rational_t *r) {
  if (!r || !r->num.proto.digits || !r->den.proto.digits) {
    return;
  }

  if (r->num.decimal_length != 0 || r->den.decimal_length != 0 ||
      r->num.repeating_length != 0 || r->den.repeating_length != 0) {
    fprintf(stderr,
            "Error: rational_normalize expects integer-only components.\n");
    return;
  }

  base_t base = r->num.proto.base;

  // If numerator is zero, canonicalize to 0/1.
  bool num_is_zero = true;
  for (size_t i = 0; i < r->num.proto.length; i++) {
    if (r->num.proto.digits[i] != 0) {
      num_is_zero = false;
      break;
    }
  }
  if (num_is_zero) {
    deallocate_number(&r->num);
    deallocate_number(&r->den);
    r->num = allocate_number_array(base, 1);
    r->den = allocate_number_array(base, 1);
    if (r->num.proto.digits) {
      r->num.proto.digits[0] = 0;
      r->num.is_negative = false;
      r->num.decimal_length = 0;
      r->num.repeating_length = 0;
    }
    if (r->den.proto.digits) {
      r->den.proto.digits[0] = 1;
      r->den.is_negative = false;
      r->den.decimal_length = 0;
      r->den.repeating_length = 0;
    }
    return;
  }

  // Compute gcd of absolute values.
  number_t abs_num = r->num;
  abs_num.is_negative = false;
  number_t g = number_int_gcd_abs(&abs_num, &r->den);

  // Divide numerator and denominator by gcd.
  number_t qn, rn;
  number_int_divmod_abs(&r->num, &g, &qn, &rn);
  number_t qd, rd;
  number_int_divmod_abs(&r->den, &g, &qd, &rd);

  // We expect zero remainders; keep quotient parts.
  deallocate_number(&r->num);
  deallocate_number(&r->den);
  deallocate_number(&rn);
  deallocate_number(&rd);
  deallocate_number(&g);
  r->num = qn;
  r->den = qd;

  // Ensure denominator sign is positive, move sign to numerator.
  if (r->den.is_negative) {
    r->den.is_negative = false;
    r->num.is_negative = !r->num.is_negative;
  }

  normalize_number(&r->num);
  normalize_number(&r->den);
}

// Add two rationals a and b (same base), returning a normalized result.
// Uses integer-only helpers: |a| * |b| and adds/subtracts with sign logic.
rational_t rational_add(const rational_t *a, const rational_t *b) {
  rational_t result;

  // Default to 0/1 in base 10 in case of early error.
  result.num = allocate_number_array(10, 1);
  result.den = allocate_number_array(10, 1);
  if (result.num.proto.digits) {
    result.num.proto.digits[0] = 0;
    result.num.is_negative = false;
    result.num.decimal_length = 0;
    result.num.repeating_length = 0;
  }
  if (result.den.proto.digits) {
    result.den.proto.digits[0] = 1;
    result.den.is_negative = false;
    result.den.decimal_length = 0;
    result.den.repeating_length = 0;
  }

  if (!a || !b || !a->num.proto.digits || !b->num.proto.digits ||
      !a->den.proto.digits || !b->den.proto.digits) {
    fprintf(stderr, "Error: rational_add requires valid rationals.\n");
    return result;
  }

  if (a->num.proto.base != b->num.proto.base ||
      a->den.proto.base != b->den.proto.base ||
      a->num.proto.base != a->den.proto.base) {
    fprintf(stderr, "Error: rational_add requires same-base components.\n");
    return result;
  }

  // Ensure integer-only components.
  if (a->num.decimal_length != 0 || a->den.decimal_length != 0 ||
      b->num.decimal_length != 0 || b->den.decimal_length != 0 ||
      a->num.repeating_length != 0 || a->den.repeating_length != 0 ||
      b->num.repeating_length != 0 || b->den.repeating_length != 0) {
    fprintf(
        stderr,
        "Error: rational_add expects integer-only numerators/denominators.\n");
    return result;
  }

  base_t base = a->num.proto.base;

  // Compute common denominator: den = a.den * b.den
  number_t den_prod = number_int_mul_abs(&a->den, &b->den);

  // Compute scaled numerators: n1 = a.num * b.den, n2 = b.num * a.den
  number_t n1_abs = number_int_mul_abs(&a->num, &b->den);
  number_t n2_abs = number_int_mul_abs(&b->num, &a->den);

  // Apply signs to n1_abs and n2_abs logically.
  n1_abs.is_negative = a->num.is_negative;
  n2_abs.is_negative = b->num.is_negative;

  // Now add the two signed integers via number_add.
  number_t n_sum = number_add(&n1_abs, &n2_abs);

  // Build result rational from n_sum / den_prod.
  rational_t tmp = rational_make_from_ints(&n_sum, &den_prod);
  rational_deallocate(&result);
  result = tmp;

  rational_normalize(&result);

  deallocate_number(&den_prod);
  deallocate_number(&n1_abs);
  deallocate_number(&n2_abs);
  deallocate_number(&n_sum);

  (void)base; // base currently unused, kept for future extensions.

  return result;
}

// Convert a terminating number_t (no repeating section) into a rational_t.
// If decimal_length == 0, returns num = x, den = 1.
// If decimal_length > 0, interprets x as N / base^d using digit arithmetic.
rational_t rational_from_terminating_number(const number_t *x) {
  rational_t r;

  // Default to 0/1 in base 10 if anything goes wrong.
  r.num = allocate_number_array(10, 1);
  r.den = allocate_number_array(10, 1);
  if (r.num.proto.digits) {
    r.num.proto.digits[0] = 0;
    r.num.is_negative = false;
    r.num.decimal_length = 0;
    r.num.repeating_length = 0;
  }
  if (r.den.proto.digits) {
    r.den.proto.digits[0] = 1;
    r.den.is_negative = false;
    r.den.decimal_length = 0;
    r.den.repeating_length = 0;
  }

  if (!x || !x->proto.digits) {
    fprintf(
        stderr,
        "Error: rational_from_terminating_number requires a valid number.\n");
    return r;
  }

  if (x->repeating_length != 0) {
    fprintf(
        stderr,
        "Error: rational_from_terminating_number expects no repeating part.\n");
    return r;
  }

  base_t base = x->proto.base;

  // Copy numerator as integer: same digits, no decimal metadata.
  deallocate_number(&r.num);
  r.num = allocate_number_array(base, x->proto.length);
  if (!r.num.proto.digits) {
    return r;
  }
  for (size_t i = 0; i < x->proto.length; i++) {
    r.num.proto.digits[i] = x->proto.digits[i];
  }
  r.num.proto.length = x->proto.length;
  r.num.is_negative = x->is_negative;
  r.num.decimal_length = 0;
  r.num.repeating_length = 0;
  normalize_number(&r.num);

  // Build denominator = base^d where d = x->decimal_length.
  deallocate_number(&r.den);
  if (x->decimal_length == 0) {
    // Just 1 in given base.
    r.den = allocate_number_array(base, 1);
    if (r.den.proto.digits) {
      r.den.proto.digits[0] = 1;
      r.den.is_negative = false;
      r.den.decimal_length = 0;
      r.den.repeating_length = 0;
    }
  } else {
    // Start with 1.
    number_t den = allocate_number_array(base, 1);
    if (!den.proto.digits) {
      return r;
    }
    den.proto.digits[0] = 1;
    den.is_negative = false;
    den.decimal_length = 0;
    den.repeating_length = 0;

    // Represent the base itself as a one-digit integer number_t.
    number_t base_num = allocate_number_array(base, 1);
    if (!base_num.proto.digits) {
      deallocate_number(&den);
      return r;
    }
    base_num.proto.digits[0] = (value_t)base;
    base_num.is_negative = false;
    base_num.decimal_length = 0;
    base_num.repeating_length = 0;
    normalize_number(&base_num);

    // Multiply den by base, decimal_length times: den *= base.
    for (size_t k = 0; k < x->decimal_length; k++) {
      number_t tmp = number_int_mul_abs(&den, &base_num);
      deallocate_number(&den);
      den = tmp;
    }

    deallocate_number(&base_num);

    r.den = den;
  }

  rational_normalize(&r);
  return r;
}

// Convert a repeating-decimal number_t into a rational_t using digit
// arithmetic. Assumes x->repeating_length > 0.
rational_t rational_from_repeating_number(const number_t *x) {
  rational_t r;

  // Default 0/1 in base 10 for error cases.
  r.num = allocate_number_array(10, 1);
  r.den = allocate_number_array(10, 1);
  if (r.num.proto.digits) {
    r.num.proto.digits[0] = 0;
    r.num.is_negative = false;
    r.num.decimal_length = 0;
    r.num.repeating_length = 0;
  }
  if (r.den.proto.digits) {
    r.den.proto.digits[0] = 1;
    r.den.is_negative = false;
    r.den.decimal_length = 0;
    r.den.repeating_length = 0;
  }

  if (!x || !x->proto.digits) {
    fprintf(stderr,
            "Error: rational_from_repeating_number requires a valid number.\n");
    return r;
  }

  if (x->repeating_length == 0) {
    fprintf(stderr,
            "Error: rational_from_repeating_number expects repeating part.\n");
    return r;
  }

  base_t base = x->proto.base;

  // Split lengths.
  size_t total_len = x->proto.length;
  size_t frac_len = x->decimal_length;
  size_t rep_len = x->repeating_length;
  if (frac_len < rep_len) {
    fprintf(stderr, "Error: invalid repeating configuration in number_t.\n");
    return r;
  }
  size_t nonrep_len = frac_len - rep_len;
  size_t int_len = total_len - frac_len;

  // Build integer M: all digits up to end of non-repeating fractional part.
  size_t M_len = int_len + nonrep_len;
  number_t M = allocate_number_array(base, M_len);
  if (!M.proto.digits) {
    return r;
  }
  for (size_t i = 0; i < M_len; i++) {
    M.proto.digits[i] = x->proto.digits[i];
  }
  M.proto.length = M_len;
  M.is_negative = false;
  M.decimal_length = 0;
  M.repeating_length = 0;
  normalize_number(&M);

  // Build integer N: all digits through one full repeating block.
  size_t N_len = total_len;
  number_t N = allocate_number_array(base, N_len);
  if (!N.proto.digits) {
    deallocate_number(&M);
    return r;
  }
  for (size_t i = 0; i < N_len; i++) {
    N.proto.digits[i] = x->proto.digits[i];
  }
  N.proto.length = N_len;
  N.is_negative = false;
  N.decimal_length = 0;
  N.repeating_length = 0;
  normalize_number(&N);

  // Compute numerator: num = N - M.
  // Ensure |N| >= |M|; given the construction, N should be >= M.
  number_t num_abs = number_int_sub_abs(&N, &M, false);
  deallocate_number(&N);
  deallocate_number(&M);

  // Build denominator: den = base^{nonrep_len} * (base^{rep_len} - 1).

  // Helper to compute base^k as integer number_t using number_int_mul_abs.
  number_t pow_base = allocate_number_array(base, 1);
  if (!pow_base.proto.digits) {
    deallocate_number(&num_abs);
    return r;
  }
  pow_base.proto.digits[0] = 1;
  pow_base.is_negative = false;
  pow_base.decimal_length = 0;
  pow_base.repeating_length = 0;

  number_t base_num = allocate_number_array(base, 1);
  if (!base_num.proto.digits) {
    deallocate_number(&num_abs);
    deallocate_number(&pow_base);
    return r;
  }
  base_num.proto.digits[0] = (value_t)base;
  base_num.is_negative = false;
  base_num.decimal_length = 0;
  base_num.repeating_length = 0;
  normalize_number(&base_num);

  // pow_b_nonrep = base^{nonrep_len}
  number_t pow_b_nonrep = pow_base;
  for (size_t k = 0; k < nonrep_len; k++) {
    number_t tmp = number_int_mul_abs(&pow_b_nonrep, &base_num);
    deallocate_number(&pow_b_nonrep);
    pow_b_nonrep = tmp;
  }

  // pow_b_rep = base^{rep_len}
  number_t pow_b_rep = allocate_number_array(base, 1);
  if (!pow_b_rep.proto.digits) {
    deallocate_number(&num_abs);
    deallocate_number(&pow_b_nonrep);
    deallocate_number(&base_num);
    return r;
  }
  pow_b_rep.proto.digits[0] = 1;
  pow_b_rep.is_negative = false;
  pow_b_rep.decimal_length = 0;
  pow_b_rep.repeating_length = 0;
  for (size_t k = 0; k < rep_len; k++) {
    number_t tmp = number_int_mul_abs(&pow_b_rep, &base_num);
    deallocate_number(&pow_b_rep);
    pow_b_rep = tmp;
  }

  // pow_b_rep_minus_one = base^{rep_len} - 1
  number_t one = allocate_number_array(base, 1);
  if (!one.proto.digits) {
    deallocate_number(&num_abs);
    deallocate_number(&pow_b_nonrep);
    deallocate_number(&pow_b_rep);
    deallocate_number(&base_num);
    return r;
  }
  one.proto.digits[0] = 1;
  one.is_negative = false;
  one.decimal_length = 0;
  one.repeating_length = 0;

  number_t pow_b_rep_minus_one = number_int_sub_abs(&pow_b_rep, &one, false);
  deallocate_number(&pow_b_rep);
  deallocate_number(&one);

  // den = pow_b_nonrep * pow_b_rep_minus_one
  number_t den_abs = number_int_mul_abs(&pow_b_nonrep, &pow_b_rep_minus_one);
  deallocate_number(&pow_b_nonrep);
  deallocate_number(&pow_b_rep_minus_one);
  deallocate_number(&base_num);

  // Assemble rational from num_abs and den_abs, then apply original sign.
  rational_t tmp = rational_make_from_ints(&num_abs, &den_abs);
  rational_deallocate(&r);
  r = tmp;

  if (x->is_negative) {
    r.num.is_negative = !r.num.is_negative;
  }

  rational_normalize(&r);

  deallocate_number(&num_abs);
  deallocate_number(&den_abs);

  return r;
}

// Compare absolute values of two numbers in the same base.
// Returns: -1 if |a| < |b|, 0 if equal, 1 if |a| > |b|.
int compare_abs(const number_t *a, const number_t *b) {
  if (!a || !b || !a->proto.digits || !b->proto.digits) {
    return 0;
  }

  // Align decimal lengths by concept: integer parts then fractional parts.
  size_t a_int_len = a->proto.length - a->decimal_length;
  size_t b_int_len = b->proto.length - b->decimal_length;

  if (a_int_len < b_int_len) {
    return -1;
  } else if (a_int_len > b_int_len) {
    return 1;
  }

  // Same integer length: compare integer digits from most significant.
  for (size_t i = 0; i < a_int_len; i++) {
    value_t da = a->proto.digits[i];
    value_t db = b->proto.digits[i];
    if (da < db) {
      return -1;
    } else if (da > db) {
      return 1;
    }
  }

  // Compare fractional part by padding shorter with zeros conceptually.
  size_t max_frac = a->decimal_length > b->decimal_length ? a->decimal_length
                                                          : b->decimal_length;
  for (size_t k = 0; k < max_frac; k++) {
    size_t a_idx = a_int_len + k;
    size_t b_idx = b_int_len + k;
    value_t da = (k < a->decimal_length && a_idx < a->proto.length)
                     ? a->proto.digits[a_idx]
                     : 0;
    value_t db = (k < b->decimal_length && b_idx < b->proto.length)
                     ? b->proto.digits[b_idx]
                     : 0;
    if (da < db) {
      return -1;
    } else if (da > db) {
      return 1;
    }
  }

  return 0;
}

// Core digit-wise addition of same-sign numbers in the same base.
// Handles decimal alignment and currently requires no repeating sections
// (repeating_length must be 0 for both operands).
number_t add_same_sign(const number_t *a, const number_t *b) {
  number_t result = allocate_number_array(a ? a->proto.base : 10, 0);

  if (!a || !b || !a->proto.digits || !b->proto.digits ||
      a->proto.base != b->proto.base) {
    fprintf(
        stderr,
        "Error: add_same_sign requires two valid numbers in the same base.\n");
    return result;
  }

  base_t base = a->proto.base;

  if (a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(stderr,
            "Error: add_same_sign does not yet support repeating decimals.\n");
    return result;
  }

  size_t a_int_len = a->proto.length - a->decimal_length;
  size_t b_int_len = b->proto.length - b->decimal_length;
  size_t max_int_len = a_int_len > b_int_len ? a_int_len : b_int_len;
  size_t max_frac_len = a->decimal_length > b->decimal_length
                            ? a->decimal_length
                            : b->decimal_length;

  // Result length at most max_int_len + max_frac_len + 1 (possible carry).
  size_t max_total = max_int_len + max_frac_len + 1;
  result = allocate_number_array(base, max_total);
  if (!result.proto.digits) {
    return result;
  }

  // Work from least significant digit to most.
  long carry = 0;
  size_t ri = max_total;

  // Fractional part (from rightmost fractional digit).
  for (size_t k = 0; k < max_frac_len; k++) {
    size_t a_pos = a->proto.length - 1 - k;
    size_t b_pos = b->proto.length - 1 - k;
    value_t da = (k < a->decimal_length && a_pos < a->proto.length)
                     ? a->proto.digits[a_pos]
                     : 0;
    value_t db = (k < b->decimal_length && b_pos < b->proto.length)
                     ? b->proto.digits[b_pos]
                     : 0;
    long sum = (long)da + (long)db + carry;
    carry = sum / base;
    value_t digit = (value_t)(sum % base);
    ri--;
    result.proto.digits[ri] = digit;
  }

  // Integer part.
  for (size_t k = 0; k < max_int_len; k++) {
    size_t a_pos = (a_int_len > k) ? (a_int_len - 1 - k) : (size_t)-1;
    size_t b_pos = (b_int_len > k) ? (b_int_len - 1 - k) : (size_t)-1;
    value_t da =
        (a_int_len > k && a_pos < a->proto.length) ? a->proto.digits[a_pos] : 0;
    value_t db =
        (b_int_len > k && b_pos < b->proto.length) ? b->proto.digits[b_pos] : 0;
    long sum = (long)da + (long)db + carry;
    carry = sum / base;
    value_t digit = (value_t)(sum % base);
    ri--;
    result.proto.digits[ri] = digit;
  }

  // Final carry.
  if (carry != 0) {
    ri--;
    result.proto.digits[ri] = (value_t)carry;
  }

  // Shift result to start at index 0.
  size_t used = max_total - ri;
  for (size_t i = 0; i < used; i++) {
    result.proto.digits[i] = result.proto.digits[ri + i];
  }

  result.proto.length = used;
  result.is_negative = a->is_negative; // both have same sign
  result.decimal_length = max_frac_len;
  result.repeating_length = 0; // conservative for now

  normalize_number(&result);
  return result;
}

// Core digit-wise subtraction of same-sign numbers in the same base.
// Computes |a| - |b| assuming |a| >= |b| in magnitude.
number_t sub_same_sign_abs(const number_t *a, const number_t *b,
                           bool negative_result) {
  number_t result = allocate_number_array(a ? a->proto.base : 10, 0);

  if (!a || !b || !a->proto.digits || !b->proto.digits ||
      a->proto.base != b->proto.base) {
    fprintf(stderr, "Error: sub_same_sign_abs requires two valid numbers in "
                    "the same base.\n");
    return result;
  }

  base_t base = a->proto.base;

  if (a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(
        stderr,
        "Error: sub_same_sign_abs does not yet support repeating decimals.\n");
    return result;
  }

  size_t a_int_len = a->proto.length - a->decimal_length;
  size_t b_int_len = b->proto.length - b->decimal_length;
  size_t max_int_len = a_int_len > b_int_len ? a_int_len : b_int_len;
  size_t max_frac_len = a->decimal_length > b->decimal_length
                            ? a->decimal_length
                            : b->decimal_length;

  size_t max_total = max_int_len + max_frac_len;
  result = allocate_number_array(base, max_total);
  if (!result.proto.digits) {
    return result;
  }

  long borrow = 0;
  size_t ri = max_total;

  // Fractional part.
  for (size_t k = 0; k < max_frac_len; k++) {
    size_t a_pos = a->proto.length - 1 - k;
    size_t b_pos = b->proto.length - 1 - k;
    long da = (k < a->decimal_length && a_pos < a->proto.length)
                  ? (long)a->proto.digits[a_pos]
                  : 0;
    long db = (k < b->decimal_length && b_pos < b->proto.length)
                  ? (long)b->proto.digits[b_pos]
                  : 0;
    long diff = da - db - borrow;
    if (diff < 0) {
      diff += base;
      borrow = 1;
    } else {
      borrow = 0;
    }
    ri--;
    result.proto.digits[ri] = (value_t)diff;
  }

  // Integer part.
  for (size_t k = 0; k < max_int_len; k++) {
    size_t a_pos = (a_int_len > k) ? (a_int_len - 1 - k) : (size_t)-1;
    size_t b_pos = (b_int_len > k) ? (b_int_len - 1 - k) : (size_t)-1;
    long da = (a_int_len > k && a_pos < a->proto.length)
                  ? (long)a->proto.digits[a_pos]
                  : 0;
    long db = (b_int_len > k && b_pos < b->proto.length)
                  ? (long)b->proto.digits[b_pos]
                  : 0;
    long diff = da - db - borrow;
    if (diff < 0) {
      diff += base;
      borrow = 1;
    } else {
      borrow = 0;
    }
    ri--;
    result.proto.digits[ri] = (value_t)diff;
  }

  // Shift result to index 0.
  size_t used = max_total - ri;
  for (size_t i = 0; i < used; i++) {
    result.proto.digits[i] = result.proto.digits[ri + i];
  }

  result.proto.length = used;
  result.is_negative = negative_result;
  result.decimal_length = max_frac_len;
  result.repeating_length = 0;

  normalize_number(&result);
  return result;
}

// Public addition API: algebraic sum of two numbers in the same base.
number_t number_add(const number_t *a, const number_t *b) {
  if (!a || !b || !a->proto.digits || !b->proto.digits) {
    fprintf(stderr,
            "Error: number_add requires non-null numbers with digits.\n");
    return allocate_number_array(10, 0);
  }

  if (a->proto.base != b->proto.base) {
    fprintf(stderr,
            "Error: number_add requires both numbers in the same base.\n");
    return allocate_number_array(a->proto.base, 0);
  }

  // If either is effectively NaN-like, return empty result.
  if (a->proto.length == 0 || b->proto.length == 0) {
    fprintf(stderr, "Error: number_add received empty number.\n");
    return allocate_number_array(a->proto.base, 0);
  }

  // For now, we only support terminating decimals (no repeating sections).
  if (a->repeating_length != 0 || b->repeating_length != 0) {
    fprintf(stderr,
            "Error: number_add does not yet support repeating decimals.\n");
    return allocate_number_array(a->proto.base, 0);
  }

  // Same sign: do addition and keep sign.
  if (a->is_negative == b->is_negative) {
    return add_same_sign(a, b);
  }

  // Different signs: compute difference of absolute values.
  int cmp = compare_abs(a, b);
  if (cmp == 0) {
    number_t zero = allocate_number_array(a->proto.base, 1);
    if (zero.proto.digits) {
      zero.proto.digits[0] = 0;
      zero.is_negative = false;
      zero.decimal_length = 0;
      zero.repeating_length = 0;
    }
    return zero;
  }

  if (cmp > 0) {
    // |a| > |b|, result sign is sign of a.
    return sub_same_sign_abs(a, b, a->is_negative);
  } else {
    // |b| > |a|, result sign is sign of b.
    return sub_same_sign_abs(b, a, b->is_negative);
  }
}

// Normalize number by removing leading and trailing insignificant zeros
void normalize_number(number_t *num) {
  if (num == NULL || num->proto.digits == NULL || num->proto.length == 0) {
    return;
  }

  // Check if repeating section contains only zeros - if so, remove it
  if (num->repeating_length > 0) {
    size_t repeating_start = num->proto.length - num->repeating_length;
    bool all_zeros_in_repeating = true;
    for (size_t i = repeating_start; i < num->proto.length; i++) {
      if (num->proto.digits[i] != 0) {
        all_zeros_in_repeating = false;
        break;
      }
    }
    if (all_zeros_in_repeating) {
      // Remove repeating section (only if ALL zeros)
      num->proto.length -= num->repeating_length;
      num->decimal_length -= num->repeating_length;
      num->repeating_length = 0;
    }
    // Do NOT trim zeros within non-zero repeating sections
    // The pattern must be preserved exactly as-is
  }

  // Find first non-zero digit (skip leading zeros)
  size_t first_nonzero = 0;
  size_t decimal_pos = num->proto.length - num->decimal_length;

  // Only skip leading zeros before the decimal point
  while (first_nonzero < decimal_pos && num->proto.digits[first_nonzero] == 0) {
    first_nonzero++;
  }

  // If all digits before decimal are zero, keep one zero
  if (first_nonzero == decimal_pos && num->decimal_length > 0) {
    first_nonzero = decimal_pos - 1;
  }

  // Find last non-zero digit (skip trailing zeros after decimal)
  size_t last_nonzero = num->proto.length - 1;

  // Only skip trailing zeros in the non-repeating decimal part
  if (num->decimal_length > 0 && num->repeating_length == 0) {
    while (last_nonzero >= decimal_pos &&
           num->proto.digits[last_nonzero] == 0) {
      if (last_nonzero == 0)
        break;
      last_nonzero--;
    }
    // If all decimal digits are zero, remove the decimal part
    if (last_nonzero < decimal_pos) {
      num->decimal_length = 0;
      last_nonzero = decimal_pos - 1;
    }
  }

  // Calculate new length
  size_t new_length = last_nonzero - first_nonzero + 1;

  // Special case: if all zeros, keep one zero
  if (new_length == 0 ||
      (new_length == 1 && num->proto.digits[first_nonzero] == 0)) {
    num->proto.digits[0] = 0;
    num->proto.length = 1;
    num->decimal_length = 0;
    num->repeating_length = 0;
    num->is_negative = false; // -0 becomes 0
    return;
  }

  // Shift digits if needed
  if (first_nonzero > 0) {
    for (size_t i = 0; i < new_length; i++) {
      num->proto.digits[i] = num->proto.digits[first_nonzero + i];
    }
  }

  // Update metadata
  size_t old_decimal_start = decimal_pos;
  size_t new_decimal_start = (old_decimal_start > first_nonzero)
                                 ? (old_decimal_start - first_nonzero)
                                 : 0;

  num->proto.length = new_length;

  if (num->decimal_length > 0) {
    num->decimal_length = new_length - new_decimal_start;
  }

  // Adjust repeating length if trailing zeros were in decimal part
  if (num->repeating_length > 0) {
    size_t new_repeating_start = new_length - num->repeating_length;
    if (new_repeating_start < new_decimal_start) {
      num->repeating_length = 0; // Repeating section was trimmed
    }
  }
}

// number display function (for debugging)
void display_number(const number_t *num) {
  if (num == NULL || num->proto.digits == NULL || num->proto.length == 0) {
    printf("<<NaN>>\n");
    return;
  }

  // Print base prefix if not base 10
  if (num->proto.base != 10) {
    printf("%u#", num->proto.base);
  }

  if (num->is_negative) {
    printf("-");
  }

  size_t decimal_pos = num->proto.length - num->decimal_length;
  size_t repeating_pos = num->proto.length - num->repeating_length;

  for (size_t i = 0; i < num->proto.length; i++) {
    // Decimal point handling (before repeating parenthesis if at same position)
    if (num->decimal_length > 0 && i == decimal_pos) {
      printf(".");
    }

    // Repeating decimals handling (after decimal point if at same position)
    if (num->repeating_length > 0 && i == repeating_pos) {
      printf("(");
    }

    // Convert digit value to glyph for display
    glyph_t glyph = value_to_glyph(num->proto.digits[i]);
    if (glyph != INVALID_GLYPH) {
      printf("%c", glyph);
    } else {
      printf("?");
    }
  }
  // Close repeating section if needed
  if (num->repeating_length > 0) {
    printf(")");
  }
  printf("\n");
}

// Number initialization function from formatted string e.g. "1A.3(45)" for base
// 16 Note: This is a simplified version and does not handle all edge cases
number_t initialize_number_from_string(const char *str, base_t base) {
  // First pass: count valid digits and validate format
  size_t length_digits = 0;
  bool seen_negative = false;
  bool seen_decimal = false;
  size_t digits_after_decimal = 0;
  size_t digits_in_repeating = 0;
  int paren_depth = 0;
  bool in_repeating = false;
  bool closed_repeating = false; // Track if we've closed a repeating section

  for (size_t i = 0; str[i] != '\0'; i++) {
    char ch = str[i];

    if (ch == '-') {
      // Negative sign must be at the start
      if (i != 0) {
        fprintf(stderr,
                "Error: Negative sign must be at the start of the number\n");
        return allocate_number_array(base, 0); // Return empty number
      }
      if (seen_negative) {
        fprintf(stderr, "Error: Multiple negative signs not allowed\n");
        return allocate_number_array(base, 0);
      }
      seen_negative = true;
      continue;
    } else if (ch == '.') {
      if (seen_decimal) {
        fprintf(stderr, "Error: Multiple decimal points not allowed\n");
        return allocate_number_array(base, 0);
      }
      // Check if there are digits before the decimal point
      if (length_digits == 0) {
        fprintf(
            stderr,
            "Error: Decimal point must have at least one digit before it\n");
        return allocate_number_array(base, 0);
      }
      seen_decimal = true;
      continue;
    } else if (ch == '(') {
      paren_depth++;
      if (paren_depth > 1) {
        fprintf(stderr, "Error: Nested parentheses not allowed\n");
        return allocate_number_array(base, 0);
      }
      if (!seen_decimal) {
        fprintf(stderr,
                "Error: Repeating section must come after decimal point\n");
        return allocate_number_array(base, 0);
      }
      if (closed_repeating) {
        fprintf(stderr, "Error: No digits allowed after closing parenthesis\n");
        return allocate_number_array(base, 0);
      }
      in_repeating = true;
      digits_in_repeating = 0;
      continue;
    } else if (ch == ')') {
      paren_depth--;
      if (paren_depth < 0) {
        fprintf(stderr, "Error: Closing parenthesis without opening\n");
        return allocate_number_array(base, 0);
      }
      // Check if repeating section is empty
      if (digits_in_repeating == 0) {
        fprintf(stderr, "Error: Repeating section cannot be empty\n");
        return allocate_number_array(base, 0);
      }
      in_repeating = false;
      closed_repeating = true;
      continue;
    } else {
      // Check if we're trying to add digits after closing parenthesis
      if (closed_repeating) {
        fprintf(stderr, "Error: No digits allowed after closing parenthesis\n");
        return allocate_number_array(base, 0);
      }

      value_t value = glyph_to_value((glyph_t)ch);
      if (value != INVALID_VALUE && value < base) {
        length_digits++;
        if (seen_decimal) {
          digits_after_decimal++;
        }
        if (in_repeating) {
          digits_in_repeating++;
        }
      } else {
        fprintf(stderr, "Error: Invalid character '%c' for base %u\n", ch,
                (unsigned int)base);
        return allocate_number_array(base, 0);
      }
    }
  }

  // Check for unclosed parentheses
  if (paren_depth != 0) {
    fprintf(stderr, "Error: Unclosed parenthesis\n");
    return allocate_number_array(base, 0);
  }

  // Check if decimal point has digits after it
  if (seen_decimal && digits_after_decimal == 0) {
    fprintf(stderr,
            "Error: Decimal point must have at least one digit after it\n");
    return allocate_number_array(base, 0);
  }

  // Check for empty input
  if (length_digits == 0) {
    fprintf(stderr, "Error: Number must contain at least one digit\n");
    return allocate_number_array(base, 0);
  }

  number_t num = allocate_number_array(base, length_digits);

  // Check if allocation failed
  if (num.proto.digits == NULL) {
    return num; // Return empty number on allocation failure
  }

  size_t digit_index = 0;
  bool in_repeating_second_pass = false;
  seen_decimal = false; // Reset for second pass

  // Second pass: parse the string and populate the number
  for (size_t i = 0; str[i] != '\0'; i++) {
    char ch = str[i];
    if (ch == '-') {
      num.is_negative = true;
    } else if (ch == '.') {
      num.decimal_length = length_digits - digit_index;
      seen_decimal = true;
    } else if (ch == '(') {
      in_repeating_second_pass = true;
    } else if (ch == ')') {
      in_repeating_second_pass = false;
    } else {
      value_t value = glyph_to_value((glyph_t)ch);
      if (value != INVALID_VALUE && value < base) {
        num.proto.digits[digit_index++] = value;
        if (in_repeating_second_pass) {
          num.repeating_length++;
        }
      }
    }
  }
  num.proto.length = digit_index; // Update length to actual number of digits

  // Normalize: remove leading and trailing insignificant zeros
  normalize_number(&num);

  return num;
}

// value_t to glyph_t conversion function
glyph_t value_to_glyph(value_t value) {
  if (value <= 9) {
    return '0' + value;
  } else if (value >= 10 && value < MAX_EXT_DIGITS) {
    return 'A' + (value - 10);
  } else {
    // Invalid input : invalid glyph
    return INVALID_GLYPH; // Using 255 to indicate invalid glyph for uint8_t
  }
}

// glyph_t to value_t conversion function
value_t glyph_to_value(glyph_t glyph) {
  if (glyph >= '0' && glyph <= '9') {
    return glyph - '0';
  } else if (glyph >= 'A' && glyph <= 'Z') {
    return glyph - 'A' + 10;
  } else if (glyph >= 'a' && glyph <= 'z') {
    return glyph - 'a' + 10;
  } else {
    // Invalid input : invalid value
    return INVALID_VALUE; // Using 255 to indicate invalid value for uint8_t
  }
}

// REPL function - Read Evaluate Print Loop
void repl() {
  printf("=== Math REPL ===\n");
  printf("Enter numbers in format: base#number (e.g., 16#1A.3(45))\n");
  printf("Default base is 10 if no prefix (e.g., 123.45)\n");
  printf("Addition: + a b  (e.g., + 1.2 0.8)\n");
  printf("Type 'exit' to quit\n\n");

  char input[256];
  while (1) {
    printf("> ");
    fflush(stdout);

    // READ: Get input from stdin
    if (fgets(input, sizeof(input), stdin) == NULL) {
      break; // EOF or error
    }

    // Remove trailing newline
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
      input[len - 1] = '\0';
      len--;
    }

    // Check for exit command
    if (strcmp(input, "exit") == 0) {
      printf("Exiting REPL\n");
      break;
    }

    // Skip empty lines
    if (len == 0) {
      continue;
    }

    // Check for addition command: starts with '+'
    if (input[0] == '+' && (input[1] == ' ' || input[1] == '\t')) {
      char *first = input + 1;
      while (*first == ' ' || *first == '\t') {
        first++;
      }
      if (*first == '\0') {
        fprintf(stderr, "Error: Addition requires two operands.\n");
        continue;
      }

      // Split first and second operand by whitespace.
      char *second = first;
      while (*second != '\0' && *second != ' ' && *second != '\t') {
        second++;
      }
      if (*second == '\0') {
        fprintf(stderr, "Error: Addition requires two operands.\n");
        continue;
      }
      *second = '\0';
      second++;
      while (*second == ' ' || *second == '\t') {
        second++;
      }
      if (*second == '\0') {
        fprintf(stderr, "Error: Addition requires two operands.\n");
        continue;
      }

      // Determine base: use explicit prefix on each operand if present,
      // otherwise default to base 10.
      base_t base = 10;

      char first_buf[256];
      char second_buf[256];

      // Helper lambda-like blocks via local scopes.
      {
        char *hash_pos = strchr(first, '#');
        if (hash_pos != NULL) {
          *hash_pos = '\0';
          int parsed_base = atoi(first);
          if (parsed_base < 2 || parsed_base > 36) {
            fprintf(stderr,
                    "Error: Base for first operand must be between 2 and 36\n");
            continue;
          }
          base = (base_t)parsed_base;
          strncpy(first_buf, hash_pos + 1, sizeof(first_buf) - 1);
          first_buf[sizeof(first_buf) - 1] = '\0';
        } else {
          strncpy(first_buf, first, sizeof(first_buf) - 1);
          first_buf[sizeof(first_buf) - 1] = '\0';
        }
      }

      {
        char *hash_pos = strchr(second, '#');
        if (hash_pos != NULL) {
          *hash_pos = '\0';
          int parsed_base = atoi(second);
          if (parsed_base < 2 || parsed_base > 36) {
            fprintf(
                stderr,
                "Error: Base for second operand must be between 2 and 36\n");
            continue;
          }
          if (base == 10) {
            base = (base_t)parsed_base;
          } else if (base != (base_t)parsed_base) {
            fprintf(stderr, "Error: Both operands must use the same base.\n");
            continue;
          }
          strncpy(second_buf, hash_pos + 1, sizeof(second_buf) - 1);
          second_buf[sizeof(second_buf) - 1] = '\0';
        } else {
          strncpy(second_buf, second, sizeof(second_buf) - 1);
          second_buf[sizeof(second_buf) - 1] = '\0';
        }
      }

      number_t a = initialize_number_from_string(first_buf, base);
      number_t b = initialize_number_from_string(second_buf, base);

      if (!a.proto.digits || !b.proto.digits) {
        fprintf(stderr, "Error: Failed to parse operands for addition.\n");
        deallocate_number(&a);
        deallocate_number(&b);
        continue;
      }

      number_t sum = number_add(&a, &b);

      printf("  ");
      display_number(&sum);

      deallocate_number(&a);
      deallocate_number(&b);
      deallocate_number(&sum);
      continue;
    }

    // Fallback: single number echo, as before.
    char *hash_pos = strchr(input, '#');
    char number_str[256];
    unsigned int base = 10; // Default base

    if (hash_pos != NULL) {
      *hash_pos = '\0';
      int parsed_base = atoi(input);
      if (parsed_base < 2 || parsed_base > 36) {
        fprintf(stderr, "Error: Base must be between 2 and 36\n");
        continue;
      }
      base = parsed_base;
      strncpy(number_str, hash_pos + 1, sizeof(number_str) - 1);
      number_str[sizeof(number_str) - 1] = '\0';
    } else {
      strncpy(number_str, input, sizeof(number_str) - 1);
      number_str[sizeof(number_str) - 1] = '\0';
    }

    number_t num = initialize_number_from_string(number_str, (base_t)base);

    printf("  ");
    display_number(&num);
    deallocate_number(&num);
  }
}

int main(int argc, char *argv[]) {

  // Check for REPL mode
  if (argc > 1 && strcmp(argv[1], "repl") == 0) {
    repl();
    return 0;
  }

  // MAX_EXT_DIGITS
  printf("MAX_EXT_DIGITS: %d\n", MAX_EXT_DIGITS);

  glyph_t sample_glyph = '3'; // Example usage

  printf("Sample glyph value for character '3': %u\n", sample_glyph);
  // print glyph as ASCII character
  printf("Sample glyph character for character '3': %c\n", sample_glyph);

  glyph_t glyph_range_buffer[MAX_EXT_DIGITS]; // Buffer to hold glyphs for
                                              // characters '0' to '9'
  for (value_t v = 0; v < ARRAY_LENGTH(glyph_range_buffer); v++) {
    glyph_range_buffer[v] =
        value_to_glyph(v); // Fill buffer with glyphs for characters '0' to '9'
  }

  printf("Buffer contents for characters '0' to 'Z':\n");
  for (value_t v = 0; v < ARRAY_LENGTH(glyph_range_buffer); v++) {
    printf("Character: %c, Glyph value: %u\n", glyph_range_buffer[v],
           glyph_range_buffer[v]);
  }

  printf("Buffer contents for characters '0' to 'Z':\n");
  for (value_t v = 0; v < ARRAY_LENGTH(glyph_range_buffer); v++) {
    printf("Character: %c, numeric value: %u\n", glyph_range_buffer[v],
           glyph_to_value(glyph_range_buffer[v]));
  }

  // Base 36
  // Base 36 shown as glyphs
  printf("Base 36 glyphs:\n");
  for (value_t v = 0; v < MAX_EXT_DIGITS; v++) {
    printf("%2c ", value_to_glyph(v));
  }
  printf("\n");

  // Base 36 shown as numeric values
  printf("Base 36 numeric values:\n");
  for (value_t v = 0; v < MAX_EXT_DIGITS; v++) {
    printf("%2u ", glyph_to_value(value_to_glyph(v)));
  }
  printf("\n");

  //===========================================================
  // SECTION: Testing integer helpers (number_int_add_abs/sub_abs)
  //===========================================================

  printf("\n=== Testing integer helpers ===\n");

  // Integer add: 123 + 456 = 579
  printf("Int Add 1: 123 + 456 (base 10): ");
  number_t i1 = initialize_number_from_string("123", 10);
  number_t i2 = initialize_number_from_string("456", 10);
  number_t isum = number_int_add_abs(&i1, &i2);
  display_number(&isum);
  deallocate_number(&i1);
  deallocate_number(&i2);
  deallocate_number(&isum);

  // Integer sub: 1000 - 1 = 999
  printf("Int Sub 1: 1000 - 1 (base 10): ");
  number_t j1 = initialize_number_from_string("1000", 10);
  number_t j2 = initialize_number_from_string("1", 10);
  number_t jdiff = number_int_sub_abs(&j1, &j2, false);
  display_number(&jdiff);
  deallocate_number(&j1);
  deallocate_number(&j2);
  deallocate_number(&jdiff);

  // Integer mul: 12 * 34 = 408
  printf("Int Mul 1: 12 * 34 (base 10): ");
  number_t m1 = initialize_number_from_string("12", 10);
  number_t m2 = initialize_number_from_string("34", 10);
  number_t mprod = number_int_mul_abs(&m1, &m2);
  display_number(&mprod);
  deallocate_number(&m1);
  deallocate_number(&m2);
  deallocate_number(&mprod);

  // Integer divmod: 100 / 7 = 14 remainder 2
  printf("Int DivMod 1: 100 / 7 (base 10):\n");
  number_t dnum1 = initialize_number_from_string("100", 10);
  number_t dden1 = initialize_number_from_string("7", 10);
  number_t dq1, dr1;
  number_int_divmod_abs(&dnum1, &dden1, &dq1, &dr1);
  printf("  q = ");
  display_number(&dq1);
  printf("  r = ");
  display_number(&dr1);
  deallocate_number(&dnum1);
  deallocate_number(&dden1);
  deallocate_number(&dq1);
  deallocate_number(&dr1);

  // Integer divmod: 7 / 7 = 1 remainder 0
  printf("Int DivMod 2: 7 / 7 (base 10):\n");
  number_t dnum2 = initialize_number_from_string("7", 10);
  number_t dden2 = initialize_number_from_string("7", 10);
  number_t dq2, dr2;
  number_int_divmod_abs(&dnum2, &dden2, &dq2, &dr2);
  printf("  q = ");
  display_number(&dq2);
  printf("  r = ");
  display_number(&dr2);
  deallocate_number(&dnum2);
  deallocate_number(&dden2);
  deallocate_number(&dq2);
  deallocate_number(&dr2);

  // Integer divmod: 0 / 7 = 0 remainder 0
  printf("Int DivMod 3: 0 / 7 (base 10):\n");
  number_t dnum3 = initialize_number_from_string("0", 10);
  number_t dden3 = initialize_number_from_string("7", 10);
  number_t dq3, dr3;
  number_int_divmod_abs(&dnum3, &dden3, &dq3, &dr3);
  printf("  q = ");
  display_number(&dq3);
  printf("  r = ");
  display_number(&dr3);
  deallocate_number(&dnum3);
  deallocate_number(&dden3);
  deallocate_number(&dq3);
  deallocate_number(&dr3);

  // Integer divmod: 5 / 10 = 0 remainder 5 (numerator < denominator)
  printf("Int DivMod 4: 5 / 10 (base 10):\n");
  number_t dnum4 = initialize_number_from_string("5", 10);
  number_t dden4 = initialize_number_from_string("10", 10);
  number_t dq4, dr4;
  number_int_divmod_abs(&dnum4, &dden4, &dq4, &dr4);
  printf("  q = ");
  display_number(&dq4);
  printf("  r = ");
  display_number(&dr4);
  deallocate_number(&dnum4);
  deallocate_number(&dden4);
  deallocate_number(&dq4);
  deallocate_number(&dr4);

  // Integer divmod: 1A / 3 in base 16 => 26 / 3 = 8 remainder 2
  printf("Int DivMod 5: 1A / 3 (base 16):\n");
  number_t dnum5 = initialize_number_from_string("1A", 16);
  number_t dden5 = initialize_number_from_string("3", 16);
  number_t dq5, dr5;
  number_int_divmod_abs(&dnum5, &dden5, &dq5, &dr5);
  printf("  q = ");
  display_number(&dq5);
  printf("  r = ");
  display_number(&dr5);
  deallocate_number(&dnum5);
  deallocate_number(&dden5);
  deallocate_number(&dq5);
  deallocate_number(&dr5);

  // Integer gcd: gcd(48, 18) = 6
  printf("Int GCD 1: gcd(48, 18) (base 10): ");
  number_t g1a = initialize_number_from_string("48", 10);
  number_t g1b = initialize_number_from_string("18", 10);
  number_t g1 = number_int_gcd_abs(&g1a, &g1b);
  display_number(&g1);
  deallocate_number(&g1a);
  deallocate_number(&g1b);
  deallocate_number(&g1);

  // Integer gcd: gcd(0, 42) = 42
  printf("Int GCD 2: gcd(0, 42) (base 10): ");
  number_t g2a = initialize_number_from_string("0", 10);
  number_t g2b = initialize_number_from_string("42", 10);
  number_t g2 = number_int_gcd_abs(&g2a, &g2b);
  display_number(&g2);
  deallocate_number(&g2a);
  deallocate_number(&g2b);
  deallocate_number(&g2);

  //===========================================================
  // SECTION: Testing rational helpers
  //===========================================================

  printf("\n=== Testing rational helpers ===\n");

  // Rational normalize: 48/18 -> 8/3
  printf("Rational 1: 48/18 (base 10): ");
  number_t r1n = initialize_number_from_string("48", 10);
  number_t r1d = initialize_number_from_string("18", 10);
  rational_t r1 = rational_make_from_ints(&r1n, &r1d);
  rational_normalize(&r1);
  printf("num = ");
  display_number(&r1.num);
  printf("den = ");
  display_number(&r1.den);
  rational_deallocate(&r1);
  deallocate_number(&r1n);
  deallocate_number(&r1d);

  // Rational normalize: 0/42 -> 0/1
  printf("Rational 2: 0/42 (base 10): ");
  number_t r2n = initialize_number_from_string("0", 10);
  number_t r2d = initialize_number_from_string("42", 10);
  rational_t r2 = rational_make_from_ints(&r2n, &r2d);
  rational_normalize(&r2);
  printf("num = ");
  display_number(&r2.num);
  printf("den = ");
  display_number(&r2.den);
  rational_deallocate(&r2);
  deallocate_number(&r2n);
  deallocate_number(&r2d);

  // Rational addition: 1/2 + 1/3 = 5/6
  printf("Rational 3: 1/2 + 1/3 (base 10): ");
  number_t r3n1 = initialize_number_from_string("1", 10);
  number_t r3d1 = initialize_number_from_string("2", 10);
  number_t r3n2 = initialize_number_from_string("1", 10);
  number_t r3d2 = initialize_number_from_string("3", 10);
  rational_t r3a = rational_make_from_ints(&r3n1, &r3d1);
  rational_t r3b = rational_make_from_ints(&r3n2, &r3d2);
  rational_normalize(&r3a);
  rational_normalize(&r3b);
  rational_t r3 = rational_add(&r3a, &r3b);
  printf("num = ");
  display_number(&r3.num);
  printf("den = ");
  display_number(&r3.den);
  rational_deallocate(&r3);
  rational_deallocate(&r3a);
  rational_deallocate(&r3b);
  deallocate_number(&r3n1);
  deallocate_number(&r3d1);
  deallocate_number(&r3n2);
  deallocate_number(&r3d2);

  // Rational addition with cancellation: -1/2 + 1/2 = 0/1
  printf("Rational 4: -1/2 + 1/2 (base 10): ");
  number_t r4n1 = initialize_number_from_string("-1", 10);
  number_t r4d1 = initialize_number_from_string("2", 10);
  number_t r4n2 = initialize_number_from_string("1", 10);
  number_t r4d2 = initialize_number_from_string("2", 10);
  rational_t r4a = rational_make_from_ints(&r4n1, &r4d1);
  rational_t r4b = rational_make_from_ints(&r4n2, &r4d2);
  rational_normalize(&r4a);
  rational_normalize(&r4b);
  rational_t r4 = rational_add(&r4a, &r4b);
  printf("num = ");
  display_number(&r4.num);
  printf("den = ");
  display_number(&r4.den);
  rational_deallocate(&r4);
  rational_deallocate(&r4a);
  rational_deallocate(&r4b);
  deallocate_number(&r4n1);
  deallocate_number(&r4d1);
  deallocate_number(&r4n2);
  deallocate_number(&r4d2);

  // Rational from terminating number: 12.34 -> 1234/100 in base 10
  printf("Rational 5: from 12.34 (base 10): ");
  number_t r5x = initialize_number_from_string("12.34", 10);
  rational_t r5 = rational_from_terminating_number(&r5x);
  printf("num = ");
  display_number(&r5.num);
  printf("den = ");
  display_number(&r5.den);
  rational_deallocate(&r5);
  deallocate_number(&r5x);

  // Rational from repeating number: 1.(3) -> 4/3 in base 10
  printf("Rational 6: from 1.(3) (base 10): ");
  number_t r6x = initialize_number_from_string("1.(3)", 10);
  rational_t r6 = rational_from_repeating_number(&r6x);
  printf("num = ");
  display_number(&r6.num);
  printf("den = ");
  display_number(&r6.den);
  rational_deallocate(&r6);
  deallocate_number(&r6x);

  // Rational from repeating number: 0.(3) -> 1/3 in base 10
  printf("Rational 7: from 0.(3) (base 10): ");
  number_t r7x = initialize_number_from_string("0.(3)", 10);
  rational_t r7 = rational_from_repeating_number(&r7x);
  printf("num = ");
  display_number(&r7.num);
  printf("den = ");
  display_number(&r7.den);
  rational_deallocate(&r7);
  deallocate_number(&r7x);

  //===========================================================
  // SECTION: Testing initialize_number_from_string
  //===========================================================

  // Test initialize_number_from_string
  printf("\n=== Testing initialize_number_from_string ===\n");

  // Test 1: Simple positive number in base 10
  printf("Test 1: \"123\" in base 10: ");
  number_t num1 = initialize_number_from_string("123", 10);
  display_number(&num1);
  deallocate_number(&num1);

  // Test 2: Hexadecimal number
  printf("Test 2: \"1A3F\" in base 16: ");
  number_t num2 = initialize_number_from_string("1A3F", 16);
  display_number(&num2);
  deallocate_number(&num2);

  // Test 3: Negative number
  printf("Test 3: \"-456\" in base 10: ");
  number_t num3 = initialize_number_from_string("-456", 10);
  display_number(&num3);
  deallocate_number(&num3);

  // Test 4: Number with decimal point
  printf("Test 4: \"12.34\" in base 10: ");
  number_t num4 = initialize_number_from_string("12.34", 10);
  display_number(&num4);
  deallocate_number(&num4);

  // Test 5: Negative decimal
  printf("Test 5: \"-9.8\" in base 10: ");
  number_t num5 = initialize_number_from_string("-9.8", 10);
  display_number(&num5);
  deallocate_number(&num5);

  // Test 6: Repeating decimals
  printf("Test 6: \"1.(3)\" in base 10: ");
  number_t num6 = initialize_number_from_string("1.(3)", 10);
  display_number(&num6);
  deallocate_number(&num6);

  // Test 7: Hexadecimal with decimal and repeating
  printf("Test 7: \"1A.3(45)\" in base 16: ");
  number_t num7 = initialize_number_from_string("1A.3(45)", 16);
  display_number(&num7);
  deallocate_number(&num7);

  // Test 8: Binary number
  printf("Test 8: \"1011.01\" in base 2: ");
  number_t num8 = initialize_number_from_string("1011.01", 2);
  display_number(&num8);
  deallocate_number(&num8);

  // Test 9: Base 36 with letters
  printf("Test 9: \"Z9A\" in base 36: ");
  number_t num9 = initialize_number_from_string("Z9A", 36);
  display_number(&num9);
  deallocate_number(&num9);

  // Test 10: Empty/zero
  printf("Test 10: \"0\" in base 10: ");
  number_t num10 = initialize_number_from_string("0", 10);
  display_number(&num10);
  deallocate_number(&num10);

  //===========================================================
  // SECTION: Testing validation errors
  //===========================================================

  printf("\n=== Testing validation (should show errors) ===\n");

  // Invalid test 1: Multiple decimal points
  printf("Invalid 1: \"12.3.4\" in base 10: ");
  number_t inv1 = initialize_number_from_string("12.3.4", 10);
  display_number(&inv1);
  deallocate_number(&inv1);

  // Invalid test 2: Negative sign not at start
  printf("Invalid 2: \"12-3\" in base 10: ");
  number_t inv2 = initialize_number_from_string("12-3", 10);
  display_number(&inv2);
  deallocate_number(&inv2);

  // Invalid test 3: Unclosed parenthesis
  printf("Invalid 3: \"1.(23\" in base 10: ");
  number_t inv3 = initialize_number_from_string("1.(23", 10);
  display_number(&inv3);
  deallocate_number(&inv3);

  // Invalid test 4: Invalid character for base
  printf("Invalid 4: \"1A3\" in base 10: ");
  number_t inv4 = initialize_number_from_string("1A3", 10);
  display_number(&inv4);
  deallocate_number(&inv4);

  // Invalid test 5: Repeating without decimal
  printf("Invalid 5: \"12(3)\" in base 10: ");
  number_t inv5 = initialize_number_from_string("12(3)", 10);
  display_number(&inv5);
  deallocate_number(&inv5);

  // Invalid test 6: Decimal at beginning
  printf("Invalid 6: \".123\" in base 10: ");
  number_t inv6 = initialize_number_from_string(".123", 10);
  display_number(&inv6);
  deallocate_number(&inv6);

  // Invalid test 7: Decimal at end
  printf("Invalid 7: \"123.\" in base 10: ");
  number_t inv7 = initialize_number_from_string("123.", 10);
  display_number(&inv7);
  deallocate_number(&inv7);

  // Invalid test 8: Empty string / only special chars
  printf("Invalid 8: \"-\" in base 10: ");
  number_t inv8 = initialize_number_from_string("-", 10);
  display_number(&inv8);
  deallocate_number(&inv8);

  // Invalid test 9: Empty parentheses
  printf("Invalid 9: \"1.()\" in base 10: ");
  number_t inv9 = initialize_number_from_string("1.()", 10);
  display_number(&inv9);
  deallocate_number(&inv9);

  // Invalid test 10: Only open paren
  printf("Invalid 10: \"(\" in base 10: ");
  number_t inv10 = initialize_number_from_string("(", 10);
  display_number(&inv10);
  deallocate_number(&inv10);

  // Invalid test 11: Only close paren
  printf("Invalid 11: \")\" in base 10: ");
  number_t inv11 = initialize_number_from_string(")", 10);
  display_number(&inv11);
  deallocate_number(&inv11);

  // Invalid test 12: Empty parens only
  printf("Invalid 12: \"()\" in base 10: ");
  number_t inv12 = initialize_number_from_string("()", 10);
  display_number(&inv12);
  deallocate_number(&inv12);

  //===========================================================

  // End of main
  return 0; // return 0 code for "success"
}