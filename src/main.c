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

// Forward declarations
glyph_t value_to_glyph(value_t value);
value_t glyph_to_value(glyph_t glyph);

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

// Forward declarations that depend on number_t
number_t initialize_number_from_string(const char *str, base_t base);
void normalize_number(number_t *num);

// Simple rational representation: sign * numerator / denominator in base 10
// (we operate on integer numerators/denominators using uint64_t where
// possible).
typedef struct {
  bool is_negative;
  uint64_t numerator;
  uint64_t denominator;
} rational_t;

// Compute greatest common divisor
static uint64_t gcd_u64(uint64_t a, uint64_t b) {
  while (b != 0) {
    uint64_t t = b;
    b = a % b;
    a = t;
  }
  return a == 0 ? 1 : a;
}

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

// Convert a base-b digit sequence with decimal and repeating information into
// a rational number (in base 10 arithmetic), using uint64_t. If the value
// overflows uint64_t, the behavior is undefined for now.
rational_t number_to_rational(const number_t *num) {
  rational_t r;
  r.is_negative = false;
  r.numerator = 0;
  r.denominator = 1;

  if (!num || !num->proto.digits || num->proto.length == 0) {
    return r;
  }

  base_t base = num->proto.base;
  size_t total_len = num->proto.length;
  size_t d = num->decimal_length;
  size_t rlen = num->repeating_length;

  // Integer and fractional decomposition uses the usual repeating-decimal
  // formula in arbitrary base b:
  // Let X = integer part, Y = non-repeating fractional, Z = repeating block.
  // Then value = X + Y / b^d + Z / (b^d * (b^rlen - 1)). We implement this by
  // turning everything into a single fraction.

  // Compute integer part X and construct full digit sequence as uint64_t.
  uint64_t value_int = 0;
  for (size_t i = 0; i < total_len; i++) {
    value_int = value_int * base + num->proto.digits[i];
  }

  // If there are no decimal digits and no repeating block, this is already an
  // integer.
  if (d == 0 && rlen == 0) {
    r.is_negative = num->is_negative;
    r.numerator = value_int;
    r.denominator = 1;
    return r;
  }

  // General case: use the standard formula with whole-number representation of
  // the non-repeating+repeating tail.
  size_t int_len = total_len - d;

  // A = digits up to decimal point (integer part)
  uint64_t A = 0;
  for (size_t i = 0; i < int_len; i++) {
    A = A * base + num->proto.digits[i];
  }

  // B_digits = integer + non-repeating fractional
  uint64_t B = 0;
  for (size_t i = 0; i < int_len + (d - rlen); i++) {
    B = B * base + num->proto.digits[i];
  }

  // C_digits = integer + full fractional (non-repeating + repeating)
  uint64_t C = 0;
  for (size_t i = 0; i < total_len; i++) {
    C = C * base + num->proto.digits[i];
  }

  if (rlen == 0) {
    // Pure terminating decimal: value = C / base^d
    uint64_t denom = 1;
    for (size_t i = 0; i < d; i++) {
      denom *= base;
    }
    uint64_t g = gcd_u64(C, denom);
    r.is_negative = num->is_negative;
    r.numerator = C / g;
    r.denominator = denom / g;
    return r;
  }

  // Repeating case in base b:
  // value = (C - B) / (b^d - b^(d - rlen))
  uint64_t pow_bd = 1;
  for (size_t i = 0; i < d; i++) {
    pow_bd *= base;
  }
  uint64_t pow_bdr = 1;
  for (size_t i = 0; i < d - rlen; i++) {
    pow_bdr *= base;
  }

  uint64_t numer = C - B;
  uint64_t denom = pow_bd - pow_bdr;
  uint64_t g = gcd_u64(numer, denom);
  numer /= g;
  denom /= g;

  r.is_negative = num->is_negative;
  r.numerator = numer;
  r.denominator = denom;
  return r;
}

// Add two rationals and return result in reduced form.
rational_t rational_add(rational_t a, rational_t b) {
  rational_t r;
  // Convert to signed 128-bit style using int64_t where possible.
  int64_t sa = a.is_negative ? -(int64_t)a.numerator : (int64_t)a.numerator;
  int64_t sb = b.is_negative ? -(int64_t)b.numerator : (int64_t)b.numerator;

  uint64_t denom = a.denominator * b.denominator;
  int64_t num_signed =
      sa * (int64_t)b.denominator + sb * (int64_t)a.denominator;

  r.is_negative = (num_signed < 0);
  uint64_t num_abs =
      (num_signed < 0) ? (uint64_t)(-num_signed) : (uint64_t)num_signed;

  uint64_t g = gcd_u64(num_abs, denom);
  r.numerator = num_abs / g;
  r.denominator = denom / g;
  return r;
}

// Convert a rational back to a base-b number_t, choosing a representation
// using a (possibly repeating) fractional part. For now we only support
// denominators that are powers of base or factors of (base^k - 1) for small k
// and fall back to a finite expansion up to a fixed precision.
number_t rational_to_number(rational_t r, base_t base) {
  // For now, handle only terminating cases where denominator is a power of
  // base; otherwise, just emit a finite expansion with a fixed maximum length
  // and no repeating block.
  const size_t MAX_DIGITS = 32;

  // Handle zero
  if (r.numerator == 0) {
    number_t zero = allocate_number_array(base, 1);
    if (zero.proto.digits) {
      zero.proto.digits[0] = 0;
      zero.is_negative = false;
      zero.decimal_length = 0;
      zero.repeating_length = 0;
    }
    return zero;
  }

  // Extract integer part
  uint64_t num = r.numerator / r.denominator;
  uint64_t rem = r.numerator % r.denominator;

  // Collect integer digits in reverse
  value_t int_digits[MAX_DIGITS];
  size_t int_len = 0;
  while (num > 0 && int_len < MAX_DIGITS) {
    value_t d = (value_t)(num % base);
    int_digits[int_len++] = d;
    num /= base;
  }
  if (int_len == 0) {
    int_digits[int_len++] = 0;
  }

  // Fractional part: generate up to MAX_DIGITS - int_len digits.
  value_t frac_digits[MAX_DIGITS];
  size_t frac_len = 0;
  while (rem != 0 && int_len + frac_len < MAX_DIGITS) {
    rem *= base;
    uint64_t digit = rem / r.denominator;
    rem = rem % r.denominator;
    frac_digits[frac_len++] = (value_t)digit;
  }

  size_t total_len = int_len + frac_len;
  number_t num_out = allocate_number_array(base, total_len);
  if (!num_out.proto.digits) {
    return num_out;
  }

  // Write integer digits in correct order
  for (size_t i = 0; i < int_len; i++) {
    num_out.proto.digits[i] = int_digits[int_len - 1 - i];
  }
  // Then fractional digits
  for (size_t i = 0; i < frac_len; i++) {
    num_out.proto.digits[int_len + i] = frac_digits[i];
  }

  num_out.proto.length = total_len;
  num_out.is_negative = r.is_negative;
  num_out.decimal_length = frac_len;
  num_out.repeating_length = 0; // conservative for now
  normalize_number(&num_out);
  return num_out;
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
// Handles decimal alignment and ignores repeating sections (assumes
// they are already expanded/truncated as desired by the caller).
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
  // Convert both to rationals in base 10 arithmetic, add them, then convert
  // back to base a->proto.base.
  rational_t ra = number_to_rational(a);
  rational_t rb = number_to_rational(b);
  rational_t rs = rational_add(ra, rb);
  return rational_to_number(rs, a->proto.base);
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