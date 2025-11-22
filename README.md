# Multi-Base Number System

A C99 project for parsing, displaying, and manipulating numbers in arbitrary bases (2-36) with support for decimal points and repeating decimals.

## Features

- **Multi-base support**: Bases 2 through 36 (using digits 0-9 and letters A-Z)
- **Decimal representation**: Support for fractional parts (e.g., `1A.3F` in base 16)
- **Repeating decimals**: Notation using parentheses (e.g., `1.(3)` represents 1.333...)
- **Negative numbers**: Standard negative sign notation
- **Input/Output symmetry**: `base#number` format for both input and output
- **Interactive REPL**: Read-Eval-Print-Loop for easy experimentation
- **Comprehensive validation**: Extensive error checking with helpful messages

## Number Format

### Syntax
```
[base#][-]digits[.digits][(repeating_digits)]
```

### Examples
- `123` - Decimal number (base 10, default)
- `16#1A3F` - Hexadecimal number
- `2#1011.01` - Binary with decimal point
- `36#Z9A` - Base 36 using letters
- `-9.8` - Negative decimal
- `1.(3)` - Repeating decimal (1.333...)
- `16#1A.3(45)` - Hex with decimal and repeating parts

### Output Format
Non-decimal numbers are displayed with the `base#` prefix:
```
> 16#FF
  16#FF
> 255
  255
```

## Building

### Requirements
- CMake 3.10 or higher
- C99-compatible compiler (GCC, Clang)
- Ninja build system (optional but recommended)

### Build Steps
```bash
# Configure
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -S . -B build

# Build
cmake --build build

# Or use the provided tasks
cmake --build build  # for the default build task
```

### VS Code Tasks
The project includes pre-configured tasks:
- **configure**: Set up CMake build system
- **build**: Compile the project
- **clean**: Remove build artifacts

## Usage

### Running Tests
```bash
./build/math
```

This runs the built-in test suite with:
- 10 valid test cases (various number formats)
- 12 invalid test cases (validation checks)

### Interactive REPL
```bash
./build/math repl
```

The REPL accepts numbers in `base#number` format:
```
=== Math REPL ===
Enter numbers in format: base#number (e.g., 16#1A.3(45))
Default base is 10 if no prefix (e.g., 123.45)
Type 'exit' to quit

> 16#1A.3(45)
  16#1A.3(45)
> 2#1011.01
  2#1011.01
> 123.45
  123.45
> exit
Exiting REPL
```

## Code Structure

### Core Types
```c
typedef uint8_t glyph_t;   // Character representation (0-9, A-Z)
typedef uint8_t value_t;   // Numeric value (0-35)
typedef uint8_t base_t;    // Number base (2-36)
```

### Main Data Structure
```c
typedef struct {
  number_proto_t proto;      // Base number data (base, digits, length)
  bool is_negative;          // Sign flag
  size_t decimal_length;     // Number of decimal digits
  size_t repeating_length;   // Number of repeating digits
} number_t;
```

### Key Functions

#### Parsing & Display
- `initialize_number_from_string(str, base)` - Parse formatted string into number_t
- `display_number(num)` - Print number with base# prefix
- `value_to_glyph(value)` - Convert numeric value to character
- `glyph_to_value(glyph)` - Convert character to numeric value

#### Memory Management
- `allocate_number_array(base, length)` - Allocate number structure
- `deallocate_number(num)` - Free allocated memory

#### REPL
- `repl()` - Interactive Read-Eval-Print-Loop

## Validation

The parser performs comprehensive validation:

1. **Negative sign** must be at the start
2. **Decimal point** requires digits before and after
3. **Repeating section** must:
   - Come after decimal point
   - Be non-empty
   - Use matching parentheses
4. **Character validation** for the specified base
5. **No nested parentheses**
6. **At least one digit** required

Error messages are written to stderr with clear descriptions.

## Development

### Code Formatting
The project uses `clang-format` with LLVM style:
```bash
clang-format -i src/main.c
```

VS Code auto-formats on save for `.c` and `.cpp` files.

### Debugging
GDB configuration is included in `.vscode/launch.json`:
- Pretty-printing enabled
- Debug build with `-g -O0`

### Compilation Flags
- Standard: C99
- Warnings: `-Wall -Wextra`
- Build type: Debug (with symbols)

## Future Enhancements

- **EVAL step**: Arithmetic operations in the REPL
- **Base conversion**: Convert between different bases
- **Math operations**: Addition, subtraction, multiplication, division
- **Precision control**: Configurable decimal places for repeating sections

## License

This project is provided as-is for educational and development purposes.

## Git History

Key commits:
- Initial implementation with parsing and display
- Memory allocation error handling
- Comprehensive input validation
- Invalid number representation (`<<NaN>>`)
- REPL functionality
- Base# prefix for input/output symmetry
