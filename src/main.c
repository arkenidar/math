// plan to use C99

#include <stdio.h>

#define MAX_GLYPHS 256
#define MAX_HEX_DIGITS 16

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
#include <stdint.h>  // For uint8_t
#include <stdlib.h>  // For malloc, free
#include <string.h>  // For string handling functions

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

// allocate number structure
number_t allocate_number_array(base_t base, size_t length) {
  number_t num;
  num.proto.base = base;
  num.proto.length = length;
  num.proto.digits = (value_t *)malloc(length * sizeof(value_t));
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

// number display function (for debugging)
void display_number(const number_t *num) {
  if (num == NULL || num->proto.digits == NULL) {
    printf("Invalid number\n");
    return;
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
  // First pass: count valid digits only
  size_t length_digits = 0;
  for (size_t i = 0; str[i] != '\0'; i++) {
    char ch = str[i];
    if (ch == '-' || ch == '.' || ch == '(' || ch == ')') {
      continue; // Skip special characters
    }
    value_t value = glyph_to_value((glyph_t)ch);
    if (value != INVALID_VALUE && value < base) {
      length_digits++;
    }
  }

  number_t num = allocate_number_array(base, length_digits);
  size_t digit_index = 0;
  bool in_repeating = false;
  bool seen_decimal = false;

  // Second pass: parse the string and populate the number
  for (size_t i = 0; str[i] != '\0'; i++) {
    char ch = str[i];
    if (ch == '-') {
      num.is_negative = true;
    } else if (ch == '.') {
      if (!seen_decimal) {
        num.decimal_length = length_digits - digit_index;
        seen_decimal = true;
      }
    } else if (ch == '(') {
      in_repeating = true;
    } else if (ch == ')') {
      in_repeating = false;
    } else {
      value_t value = glyph_to_value((glyph_t)ch);
      if (value != INVALID_VALUE && value < base) {
        num.proto.digits[digit_index++] = value;
        if (in_repeating) {
          num.repeating_length++;
        }
      }
    }
  }
  num.proto.length = digit_index; // Update length to actual number of digits
  return num;
}

// Array to hold glyph data for 256 characters (buffer for allocation purposes ,
// heap to be used later) note: maybe many of such heaps will be needed in
// future for larger memory management
//////glyph_t glyphs[256] = {0}; // Initialize all elements to 0

// value_t to glyph_t conversion function
glyph_t value_to_glyph(value_t value) {
  if (/* value >= 0 && */ value <= 9) {
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

int main() {

  // MAX_EXT_DIGITS
  printf("MAX_EXT_DIGITS: %d\n", MAX_EXT_DIGITS);

  //////printf("Size of glyphs element: %zu bytes\n", sizeof(glyphs[0]) );

  //////printf("Glyph array initialized with %zu elements.\n",
  /// ARRAY_LENGTH(glyphs));

  //////printf("Glyph for character '0' (ASCII 48): %u\n", glyphs['0']);

  // Initialize glyph data for characters 0 to 255
  ////// for (int i = 0; i < 256; i++) {
  //     glyphs[i] = (glyph_t)i; // Example initialization
  // }

  //////printf("Glyph for character '0' (ASCII 48): %u\n", glyphs['0']);

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

  // End of main
  return 0; // return 0 code for "success"
}