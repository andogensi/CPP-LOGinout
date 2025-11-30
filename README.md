# logfunc Library

*[日本語版 (Japanese)](README_JA.md)*

A simple I/O library for C++.
Easily switch data input/output between files and console.

## Features

- **Modern C++** - Supports C++17 and later (`std::filesystem`, `std::string_view`, etc.)
- **Header-only** - Just include `logfunc.h` to use
- **Simple API** - Intuitive function names (`logff`, `logc`, `loginf`, `loginc`)
- **Type-safe** - Template-based variadic arguments supporting any type
- **Stream-style** - RAII-compliant using `std::ofstream`/`std::ifstream`
- **Customizable** - `std::string`-based path management
- **Automatic file monitoring** - Update detection via `std::filesystem`
- **High performance** - Smart resource management with `std::unique_ptr`
- **Thread-safe** - Exclusive control with `std::mutex`
- **Cross-platform** - Windows/Linux compatible

---

## Usage 1: Header-only Version (Recommended)

Simply include `include/logfunc.h`.

```cpp
#include "include/logfunc.h"

int main() {
    // Modern C++: Easy output with stream style
    int x = 42;
    float pi = 3.14f;
    logff("x: ", x, ", pi: ", pi, "\n");  // Output to log.txt
    
    logc("World\n");   // Output to console
    
    int input;
    loginf(input);  // Input from in.txt (auto-detects updates via std::filesystem)
    logff("input value: ", input, "\n");
    
    return 0;
}
```

**Build instructions:**
```powershell
# MinGW/GCC (C++17 or later required)
g++ -std=c++17 examples/example.cpp -I include -o example.exe

# MSVC (C++17 or later required)
cl /std:c++17 examples/example.cpp /I include

# Run
.\example.exe
```

---

## Usage 2: Building with CMake (Recommended)

Using CMake makes it easy to integrate into other projects.

### Method A: Include with add_subdirectory

```cmake
# Your project's CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(MyProject)

# Add logfunc as a subdirectory
add_subdirectory(path/to/logfunc)

# Your executable
add_executable(my_app main.cpp)

# Use header-only version (recommended)
target_link_libraries(my_app PRIVATE logfunc::header_only)

# Or use the library version
# target_link_libraries(my_app PRIVATE logfunc::lib)
```

### Method B: Fetch directly with FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    logfunc
    GIT_REPOSITORY https://github.com/andogensi/C--LOGinout.git
    GIT_TAG main
)
FetchContent_MakeAvailable(logfunc)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE logfunc::header_only)
```

### Method C: Use with find_package (after installation)

```cmake
find_package(logfunc REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE logfunc::header_only)
```

### CMake Build Steps

```powershell
# Create build directory
mkdir build
cd build

# Configure CMake
cmake .. -G "MinGW Makefiles"  # For MinGW
# cmake .. -G "Visual Studio 17 2022"  # For Visual Studio

# Build with examples
cmake .. -DLOGFUNC_BUILD_EXAMPLES=ON

# Build
cmake --build .

# Install (optional)
cmake --install . --prefix C:/local/logfunc
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `LOGFUNC_BUILD_EXAMPLES` | OFF | Build sample programs |
| `LOGFUNC_HEADER_ONLY` | ON | Use header-only version by default |

---

## Usage 3: Library Version (For Binary Distribution)

For use as a static library.

### Building the Library

```powershell
# MinGW/GCC
g++ -std=c++17 -c src/logfunc_lib.cpp -I include -o logfunc_lib.o
ar rcs liblogfunc.a logfunc_lib.o

# MSVC
cl /std:c++17 /c src/logfunc_lib.cpp /I include /EHsc
lib logfunc_lib.obj /OUT:logfunc.lib
```

**Note:** C++17 or later is required (uses `std::filesystem`, `std::string_view`)

### Using the Library

```cpp
#include "include/logfunc_lib.h"

int main() {
    int x = 42;
    logff("Value: ", x, "\n");  // Modern C++: variadic templates
    logc("World\n");
    return 0;
}
```

**Build instructions:**
```powershell
# MinGW/GCC (C++17 or later required)
g++ -std=c++17 examples/example_lib.cpp -I include -L. -llogfunc -o example_lib.exe

# MSVC (C++17 or later required)
cl /std:c++17 examples/example_lib.cpp /I include logfunc.lib /EHsc
```

---

## Modern C++ Features

This library leverages **Modern C++ (C++17 and later)** features:

### Key Improvements

1. **`FILE*` → `std::ofstream`/`std::ifstream`**
   - Automatic resource management following RAII principles
   - Improved exception safety
   - Ownership management with smart pointers (`std::unique_ptr`)

2. **`char[]` → `std::string`**
   - No need for dynamic memory management
   - No buffer overflow concerns
   - Efficient string references with `std::string_view`

3. **`std::filesystem` Usage**
   - Platform-independent file operations
   - Accurate update detection with `last_write_time()`
   - Simplified path operations

4. **Variadic Template Arguments**
   - Type-safe implementation without `va_list`
   - Compile-time type checking
   - Natural output of any type

### Code Comparison

**Traditional C-style:**
```cpp
// Implementation using FILE*
FILE* fp = fopen("log.txt", "a");
fprintf(fp, "Value: %d\n", 42);
fclose(fp);  // Manual close required
```

**Modern C++ style:**
```cpp
// Implementation using std::ofstream
{
    std::ofstream file("log.txt", std::ios::app);
    file << "Value: " << 42 << "\n";
}  // Automatically closes when leaving scope (RAII)
```

**Using this library:**
```cpp
// Even simpler
logff("Value: ", 42, "\n");
```

---

## API Reference

### Initialization Functions

| Function | Description |
|----------|-------------|
| `init_log(std::string_view path)` | Set log file path (default: "log.txt") |
| `init_input(std::string_view path)` | Set input file path (default: "in.txt") |

### Output Functions

| Function | Description | Output Target |
|----------|-------------|---------------|
| `logff(args...)` | Output variadic arguments in stream style | Default or configured log file |
| `logto(filepath, args...)` | Output variadic arguments to specified file | Specified file |
| `logc(args...)` | Output variadic arguments to console | Console |
| `log_flush(filepath)` | Force flush buffer for specified file (all files if omitted) | - |
| `log_close_all()` | Close all file handles | - |

**Modern C++ Variadic Templates:**

This library uses `template<typename... Args>` to accept any number and type of arguments.

**Usage Examples:**
```cpp
// Natural output in stream style
int x = 42;
float y = 3.14f;
logff("x: ", x, ", y: ", y, "\n");  // "x: 42, y: 3.14" to log.txt

// String literals work naturally too
logff("Status: ", "OK", "\n");  // "Status: OK" to log.txt

// Change file path
init_log("custom.txt");
logff("Result: ", 100, "\n");  // Output to custom.txt

// Output to specific file (logto function)
logto("debug.txt", "Debug: value = ", 42, "\n");
logto("result.txt", 3.14159, "\n");
logto("data.txt", 42, "\n");

// Simple value output
logff(42, "\n");  // "42" to log.txt
logff(3.14f, "\n");  // "3.14" to log.txt

// Multiple values at once
logff("a=", 1, " b=", 2, " c=", 3, "\n");

// Standard library types are also supported
std::string name = "Alice";
logff("Name: ", name, "\n");
```

**Type Safety:**
- Type checking at compile time
- Any type with `<<` operator defined can be output
- No printf-style `%d`/`%s` mismatch errors

### Input Functions

#### Blocking Version (Legacy)

| Function | Description | Input Source |
|----------|-------------|--------------|
| `loginf(variable)` | Read one line (**blocks main thread**) | `in.txt` |
| `loginc(variable)` | Read one line | Console |

**Supported types:** `int`, `float`, `double`

**Notes:**
- `loginf()` automatically monitors `in.txt` for updates
- File is auto-created if it doesn't exist
- Lines starting with `#` are treated as comments
- ⚠️ **Main thread blocks until file is updated** (for debugging/experimentation)

#### Non-blocking Version (Recommended)

Versions that don't block the main thread (GUI/game loop).

| Function | Description | Use Case |
|----------|-------------|----------|
| `loginf_try(variable)` | Checks immediately, returns `true` if readable | Game loops, real-time processing |
| `loginf_timeout(variable, timeout)` | With timeout, returns `false` if not readable in time | Prevent long freezes |
| `loginf_async<T>()` | Waits asynchronously, returns `std::future<T>` | Background processing |
| `loginf_async<T>(callback)` | Waits asynchronously, calls callback | Event-driven |

**Usage Examples:**

```cpp
// Non-blocking: Use in game loop
int value = 0;
while (game_running) {
    if (loginf_try(value)) {
        std::cout << "Player input: " << value << "\n";
        break;
    }
    update_game();  // Not blocked!
    render_frame();
}

// Timeout version: Wait 5 seconds then timeout
if (loginf_timeout(value, std::chrono::seconds(5))) {
    std::cout << "Got: " << value << "\n";
} else {
    std::cout << "Timeout!\n";
    value = 100; // Default value
}

// Async version: Wait in background
auto future = loginf_async<int>();
// Do other processing on main thread...
int value = future.get(); // Get result

// Async version (callback): Event-driven
loginf_async<int>([](int value) {
    std::cout << "Received: " << value << "\n";
});
// Main thread continues immediately...
```

See [ASYNC_USAGE.md](ASYNC_USAGE.md) for details.

---

## Sample Code

### examples/example.cpp - Basic Usage

```cpp
#include "include/logfunc.h"

int main() {
    // Example 1: File output and file input (Modern C++ version)
    logff("input number:\n");
    int num;
    loginf(num);
    logff("you input number is: ", num, "\n");
    logc("Console output: ", num, "\n");
    
    // Example 2: Output multiple values at once
    float f;
    logff("input float number:\n");
    loginc(f);
    logff("you input float number is: ", f, "\n");
    
    // Example 3: Stream-style output
    int x = 10, y = 20;
    logff("x=", x, ", y=", y, ", sum=", x + y, "\n");
    
    return 0;
}
```

### examples/benchmark.cpp - Performance Test

```cpp
#include <iostream>
#include <chrono>
#include "../include/logfunc.h"

int main() {
    const int ITERATIONS = 10000;
    
    std::cout << "=== Performance Benchmark ===\n";
    std::cout << "Testing " << ITERATIONS << " log operations...\n\n";
    
    // High-speed logging test (Modern C++ variadic templates)
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        logff("Frame: ", i, ", X: ", i * 10, ", Y: ", i * 20, "\n");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "✓ Completed in " << duration.count() << " ms\n";
    std::cout << "✓ Throughput: " << (ITERATIONS / (duration.count() / 1000.0)) << " ops/sec\n";
    
    return 0;
}
```

**Run:**
```powershell
g++ -std=c++17 examples/benchmark.cpp -I include -o benchmark.exe
.\benchmark.exe
```

### in.txt Example

```
# Write input values in this file (one per line)
42
3.14
```

### Output

**log.txt:**
```
input number:
you input number is: 42
input float number:
you input float number is: 3.14
x=10, y=20, sum=30
```

**Console:**
```
[Waiting for input in in.txt...]
[Read value: 42]
Console output: 42
input float number:
3.14
```

---

## File Structure

```
.
├── CMakeLists.txt            # CMake build configuration
├── cmake/                    # CMake configuration files
│   ├── logfuncConfig.cmake.in
│   └── logfunc.pc.in
├── include/                  # Header files
│   ├── logfunc.h             # Header-only version (recommended)
│   └── logfunc_lib.h         # Library version header
├── src/                      # Source files
│   └── logfunc_lib.cpp       # Library version implementation
├── examples/                 # Sample programs
│   ├── example.cpp           # Header-only version sample
│   ├── example_lib.cpp       # Library version sample
├── ASYNC_USAGE.md            # Detailed async API documentation
├── LICENSE                   # License file
└── README.md                 # This file
```

---

## Important Notes

### Function Name Distinction

- **`logff()`**: Output to default log file (`log.txt`)
- **`logto()`**: Output to specific file (Log To File)

This distinction resolves ambiguities like:

```cpp
// ✅ Works correctly: Output variadic args with logff
logff("Status: ", "OK", "\n");  // "Status: OK" to log.txt

// ✅ Works correctly: Output to specific file with logto
logto("custom.txt", "Status: ", "OK", "\n");  // "Status: OK" to custom.txt
```

Previous versions had a `logff(filepath, value)` function, which caused bugs where
`logff("Status: %s", "OK")` would unintentionally try to create a file named "Status: %s".
This has been resolved by introducing the `logto` function.

---

## Performance Optimization

### File Handle Caching

Since v2.0, file handles are cached to reduce overhead from opening/closing files each time.

**Legacy (slow):**
```cpp
for (int i = 0; i < 10000; i++) {
    logff("frame: ", i, "\n");  // fopen/fclose each time → very slow
}
```

**Optimized (fast):**
```cpp
for (int i = 0; i < 10000; i++) {
    logff("frame: ", i, "\n");  // Reuses std::ofstream → fast
}
// Automatically closes on program exit (RAII)
```

**Features:**
- `std::ofstream` opens on first access, managed by `std::unique_ptr`
- Buffering disabled (`pubsetbuf`) for real-time performance
- Auto-closes on program exit via RAII
- Thread-safe with `std::mutex` locking

**When manual control is needed:**
```cpp
// Explicit flush (ensure write to disk)
log_flush();  // All files
log_flush("custom.txt");  // Specific file

// Manually close all files (usually unnecessary)
log_close_all();
```

**Measured Performance:**
```
10,000 log outputs: ~45ms (222,222 ops/sec)
Average: 0.0045ms/operation
```
Achieves **~10-50x speedup** compared to traditional open/close each time.

**Modern C++ Benefits:**
- Memory leak prevention with `std::unique_ptr`
- Reliable resource cleanup via RAII
- Improved exception safety

---

## Troubleshooting

### File Not Found

Check the header file path:
```cpp
#include "include/logfunc.h"  // From project root
```

### File Input Not Working

- Check if `in.txt` exists (auto-created if missing)
- Check file read permissions
- Comment lines (starting with `#`) and empty lines are automatically skipped

### Build Errors

For MinGW/GCC:
```powershell
g++ -std=c++17 your_program.cpp -I include -o program.exe
```

**C++17 Feature Requirements:**
- `std::filesystem`: File system operations
- `std::string_view`: Efficient string references
- Structured bindings: `auto& [key, value]`
- Template fold expressions: `(stream << ... << args)`

---

## Repository

GitHub: [andogensi/C--LOGinout](https://github.com/andogensi/C--LOGinout)

---

## License

MIT License - Free to use, modify, and distribute
