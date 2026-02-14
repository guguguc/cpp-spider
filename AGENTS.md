# AGENTS.md - Weibo Spider Project

## Project Overview

This is a Qt6-based Weibo web scraper with a graphical user interface. It collects Weibo user data, displays relationships in an interactive graph, and stores data in MongoDB. The project uses CMake as its build system.

## Build Commands

### Building the Project
```bash
cd build && make
```

The compiled binary is located at `build/cpp-spider`.

### Running
```bash
./build/cpp-spider
```

Note: Requires MongoDB running at `mongodb://0.0.0.0:27017`, a `cookie.json` file with valid Weibo cookies, and a `headers.json` file with valid Weibo session headers in the project root.

### Dependencies
- CMake 3.15+
- Qt6 (Widgets, Network, MultimediaWidgets)
- spdlog (logging)
- httplib (HTTP client)
- nlohmann_json (JSON)
- mongocxx / bsoncxx (MongoDB)
- fmt (string formatting)

### Testing
**No tests exist in this project.** If adding tests, use Google Test (gtest).

---

## Code Style Guidelines

### General Conventions

| Aspect | Rule |
|--------|------|
| Language Standard | C++17 or later |
| Build System | CMake |
| Header Guards | Use `#ifndef` / `#define` / `#endif` |
| Namespace Usage | **Never use `using namespace std`** - always use `std::` prefix explicitly |
| Indentation | 2 spaces |
| Braces | Opening brace on same line for functions/classes |
| Line Length | Keep under 120 characters when practical |

### Naming Conventions

| Type | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `Spider`, `Weibo`, `User`, `MongoWriter`, `MainWindow`, `NodeItem` |
| Member Variables | `m_` prefix + snake_case | `m_self`, `m_client`, `m_visit_cnt` |
| Regular Variables | snake_case | `user_id`, `user_name`, `get_follower` |
| Functions | snake_case | `get_user()`, `run()`, `write_one()` |
| Constants | snake_case | `mongo_url` |
| Headers | Lowercase | `spider.hpp`, `weibo.hpp`, `mainwindow.hpp` |
| Qt Classes | PascalCase with Qt prefix | `QMainWindow`, `QGraphicsScene`, `QVideoWidget` |
| Enums | PascalCase | `LayoutType::Circular` |

### Include Order

For `.cpp` files, follow this order:
1. Corresponding header: `"spider.hpp"`
2. Standard library headers: `<chrono>`, `<string>`, `<vector>`, `<memory>`, etc.
3. Third-party headers: `<httplib.h>`, `<nlohmann/json.hpp>`, `<spdlog/spdlog.h>`, `<fmt/core.h>`
4. Qt headers: `<QMainWindow>`, `<QPushButton>`, etc.
5. Local project headers: `"weibo.hpp"`, `"writer.hpp"`, `"mainwindow.hpp"`

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
#include <QMainWindow>          // Qt
#include <QPushButton>
#include "weibo.hpp"            // Local headers last
```

### Type Usage

- Use `std::string` for strings
- Use `std::vector<T>` for collections
- Use `uint64_t` for IDs and numeric identifiers
- Use `bool` for flags
- Use `std::unique_ptr<T>` for heap-allocated objects (see `m_client` in spider.cpp)
- Use `std::make_unique<T>()` for creating unique_ptr instances
- Use Qt container classes where appropriate: `QMap`, `QList`, `QVector`

### Error Handling

- Use `try-catch` blocks for exception handling
- Catch by const reference: `catch (const std::exception &e)`
- Log errors with `spdlog::error(e.what())`
- Implement retry logic with `goto` or loop constructs (see spider.cpp:95-141 for retry pattern)
- Add delays between retries using `std::this_thread::sleep_for(std::chrono::seconds(N))`

### Logging

- Use `spdlog` for logging (not Qt's logging system)
- Info: `spdlog::info(fmt::format("message {}", value))`
- Error: `spdlog::error(e.what())`
- Use `fmt::format` for string formatting (never use `+` for string concatenation)

### String Formatting

Always use `fmt::format`:
```cpp
// Good
const std::string url = fmt::format("/ajax/profile/info?uid={}", uid);
spdlog::info(fmt::format("uid: {}, user name {}", uid, name));

// Avoid
std::string url = "/ajax/profile/info?uid=" + std::to_string(uid);
```

### Qt-Specific Guidelines

- Use `std::unique_ptr<T>` for managing Qt object lifetime where possible
- Use `QMetaObject::invokeMethod` for thread-safe GUI updates from worker threads
- Connect signals/slots using the modern `connect()` syntax with function pointers
- Use Qt's container classes (`QMap`, `QList`) for Qt-related data
- Set parent widgets to manage memory automatically
- Use `Qt::QueuedConnection` when emitting signals from non-GUI threads

### MongoDB Operations

- Use `bsoncxx::builder::basic::document` and `array` for building BSON documents
- Use `mongocxx::client`, `database`, `collection` for database operations
- Always specify server API version when creating client (see writer.cpp:9-11):
```cpp
mongocxx::options::client client_option;
auto api = mongocxx::options::server_api{mongocxx::options::server_api::version::k_version_1};
client_option.server_api_opts(api);
m_client = mongocxx::client{mongocxx::uri(uri), client_option};
```

### Best Practices

1. **Const correctness**: Use `const` for parameters that are not modified
2. **Reference parameters**: Use `const &` for passing large objects
3. **Move semantics**: Use `std::move` when transferring ownership
4. **RAII**: Use RAII patterns (e.g., `mongocxx::instance inst{}`)
5. **Explicit constructors**: Use `explicit` for single-argument constructors
6. **Include what you use**: Include all headers needed for symbols used
7. **Qt parent ownership**: Let parent widgets manage child widget lifetime

### Common Patterns

```cpp
// Constructor with initialization (spider.cpp:17)
Spider::Spider(uint64_t uid) : m_writer("mongodb://0.0.0.0:27017") {
  m_visit_cnt = 0;
  m_self = User(uid, "", std::vector<User>());
  m_client = std::make_unique<httplib::Client>("https://www.weibo.com");
}

// Error handling with retry (spider.cpp:95-141)
task:
  try {
    httplib::Result resp = m_client->Get(url);
    auto json_resp = json::parse(resp->body);
  } catch (const std::exception &e) {
    spdlog::error(e.what());
    std::this_thread::sleep_for(std::chrono::seconds(10));
    goto task;  // retry
  }

// Thread-safe GUI update (mainwindow.cpp:456)
QMetaObject::invokeMethod(this, "onUserFetched", Qt::QueuedConnection,
                          Q_ARG(uint64_t, uid),
                          Q_ARG(QString, QString::fromStdString(name)));
```

### Adding New Files

1. Create `.hpp` header file with proper include guard
2. Create corresponding `.cpp` implementation file
3. Add to `CMakeLists.txt` `add_executable` command
4. Follow the include order and naming conventions above
5. Add new source files to the list in CMakeLists.txt:17

### Dependencies

When adding new dependencies:
1. Update `CMakeLists.txt` with `find_package()` for Qt or CMake modules
2. Use `pkgconfig` for non-CMake dependencies
3. Update `target_link_libraries()` in CMakeLists.txt
4. Add corresponding includes in appropriate location order

---

## Code Architecture

### Overview

The project follows a layered architecture with clear separation of concerns:
- **Presentation Layer**: Qt-based GUI (MainWindow)
- **Business Logic Layer**: Spider engine and layout algorithms
- **Data Layer**: Data models and MongoDB persistence
- **External Integration**: Weibo API via HTTP, MongoDB database

### Core Components

| Component | Files | Role |
|-----------|-------|------|
| **Spider** | `spider.cpp/hpp` | Crawling engine, HTTP requests, rate limiting |
| **MongoWriter** | `writer.cpp/hpp` | MongoDB connection and document persistence |
| **Data Models** | `weibo.cpp/hpp` | `Weibo` and `User` data structures |
| **MainWindow** | `mainwindow.cpp/hpp` | GUI orchestration, graph visualization |
| **GraphLayout** | `graph_layout.hpp` | Layout algorithms (Force, Circular, Grid, etc.) |

### Component Interaction

```
MainWindow (GUI Thread)
    │
    ├─ creates ──► Spider (Worker Thread via QThread)
    │                  │
    │                  └─ callbacks ──► MongoWriter
    │                                    │
    │                                    └─ writes ──► MongoDB
    │
    └─ uses ──────► GraphLayout (static methods)
```

### Data Flow

1. **User input** (target UID, crawl options) → MainWindow
2. **Spider runs** in worker thread:
   - Fetches user data from Weibo API
   - Callbacks update GUI via `QMetaObject::invokeMethod`
   - Callbacks persist data via MongoWriter
3. **MongoWriter** constructs BSON documents and writes to MongoDB
4. **MainWindow** visualizes data as interactive graph
5. **GraphLayout** provides node positioning algorithms

### Threading Model

- **Main Thread**: Qt GUI event loop, user interaction, rendering
- **Worker Thread**: Spider execution (HTTP requests, JSON parsing)
- **Image Loading Threads**: Detached threads for async image fetching
- **Communication**: Thread-safe via `QMetaObject::invokeMethod` with `Qt::QueuedConnection`

### MongoDB Schema

**Database**: `weibo`  
**Collection**: `user`

```javascript
{
  "uid": "1234567890",
  "username": "user_name",
  "weibos": [
    {
      "id": "post_id",
      "timestamp": "2024-01-01 12:00:00",
      "text": "post content",
      "pics": ["url1", "url2", ...],
      "video": "video_url"
    }
  ]
}
```

### External Configuration Files

| File | Purpose |
|------|---------|
| `cookie.json` | Weibo authentication cookies |
| `headers.json` | HTTP headers for API requests |
| `config.json` | User preferences (saved by MainWindow) |
