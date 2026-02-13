# AGENTS.md - C++ Spider Project

## Project Overview

This is a C++ web scraper project that collects Weibo user data and stores it in MongoDB. It uses CMake as the build system.

## Build Commands

### Building the Project
```bash
cd build && make
```

The compiled binary is located at `build/cpp-spider`.

### Dependencies
- CMake 3.15+
- spdlog (logging)
- httplib (HTTP client)
- nlohmann_json (JSON)
- mongocxx / bsoncxx (MongoDB)
- fmt (string formatting)

### Running
```bash
./build/cpp-spider
```

Note: Requires MongoDB running at `mongodb://0.0.0.0:27017` and a `build/headers.json` file with valid Weibo session headers.

### Testing
**No tests exist in this project.** If adding tests, use a framework like Google Test (gtest).

---

## Code Style Guidelines

### General Conventions

- **Language Standard**: C++17 or later
- **Build System**: CMake
- **Header Guards**: Use `#ifndef` / `#define` / `#endif` (see existing files for format)
- **No `using namespace std`** - Always use `std::` prefix explicitly

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `Spider`, `Weibo`, `User`, `MongoWriter` |
| Member Variables | `m_` prefix + snake_case | `m_self`, `m_client`, `m_visit_cnt` |
| Regular Variables | snake_case | `user_id`, `user_name`, `get_follower` |
| Functions | snake_case | `get_user()`, `run()`, `write_one()` |
| Constants | snake_case | `mongo_url` |
| Headers | Lowercase | `spider.hpp`, `weibo.hpp` |

### Include Order

1. Corresponding header (for .cpp files): `"spider.hpp"`
2. Standard library headers: `<vector>`, `<string>`, `<memory>`, etc.
3. Third-party headers: `<httplib.h>`, `<nlohmann/json.hpp>`, `<spdlog/spdlog.h>`, `<fmt/core.h>`
4. Local project headers: `"weibo.hpp"`, `"writer.hpp"`

```cpp
#include "spider.hpp"           // Corresponding header first
#include <chrono>               // STL
#include <cstdint>
#include <fmt/core.h>           // Third-party
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>
#include "weibo.hpp"             // Local headers last
```

### Formatting

- **Indentation**: Use spaces (2 or 4 based on project; default to 2 spaces for consistency with existing code)
- **Braces**: Opening brace on same line for functions/classes
- **Line length**: Keep under 120 characters when practical
- **Spacing**: Use spaces around operators (`=`, `+`, `&`), after commas, but not before unary operators

### Type Usage

- Use `std::string` for strings
- Use `std::vector<T>` for collections
- Use `uint64_t` for IDs and numeric identifiers
- Use `bool` for flags
- Use `std::unique_ptr<T>` for heap-allocated objects (see `m_client` in spider.cpp:20)
- Use `std::make_unique<T>()` for creating unique_ptr instances

### Error Handling

- Use `try-catch` blocks for exception handling
- Catch by const reference: `catch (const std::exception &e)`
- Log errors with `spdlog::error(e.what())`
- Implement retry logic with `goto` or loop constructs (see spider.cpp:42-65 for retry pattern)
- Add delays between retries using `std::this_thread::sleep_for(std::chrono::seconds(N))`

### Logging

- Use `spdlog` for all logging
- Info: `spdlog::info(fmt::format("message {}", value))`
- Error: `spdlog::error(e.what())`
- Use `fmt::format` for string formatting (do not use `+` for string concatenation)

### String Formatting

Always use `fmt::format`:
```cpp
// Good
const std::string url = fmt::format("/ajax/profile/info?uid={}", uid);
spdlog::info(fmt::format("uid: {}, user name {}", uid, name));

// Avoid
std::string url = "/ajax/profile/info?uid=" + std::to_string(uid);
```

### MongoDB Operations

- Use `bsoncxx::builder::basic::document` and `array` for building BSON documents
- Use `mongocxx::client`, `database`, `collection` for database operations
- Always specify server API version when creating client (see writer.cpp:10-11)

### Best Practices

1. **Const correctness**: Use `const` for parameters that are not modified
2. **Reference parameters**: Use `const &` for passing large objects
3. **Move semantics**: Use `std::move` when transferring ownership
4. **RAII**: Use RAII patterns (e.g., `mongocxx::instance inst{}`)
5. **Explicit constructors**: Use `explicit` for single-argument constructors to prevent implicit conversions
6. **Include what you use**: Include all headers needed for symbols used

### Common Patterns

```cpp
// Constructor with initialization
Spider::Spider(uint64_t uid) : m_writer("mongodb://0.0.0.0:27017") {
  m_visit_cnt = 0;
  m_self = User(uid, "", std::vector<User>());
  m_client = std::make_unique<httplib::Client>("https://weibo.com");
}

// Error handling with retry
try {
  httplib::Result resp = m_client->Get(url);
  auto json_resp = json::parse(resp->body);
  // process response
} catch (const std::exception &e) {
  spdlog::error(e.what());
  std::this_thread::sleep_for(std::chrono::seconds(10));
  goto task;  // retry
}
```

### Adding New Files

1. Create `.hpp` header file with proper include guard
2. Create corresponding `.cpp` implementation file
3. Add to `CMakeLists.txt` `add_executable` command
4. Follow the include order and naming conventions above

### Dependencies

When adding new dependencies:
1. Update `CMakeLists.txt` with `find_package()` or pkg-config
2. Update `target_link_libraries()` in CMakeLists.txt
3. Add corresponding include in appropriate location
