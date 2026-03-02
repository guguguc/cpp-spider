# cpp-spider

A Qt6 desktop Weibo scraper and visualizer.

It fetches user profile data from Weibo, draws relationship graphs, collects Weibo posts (text/images/videos), and writes crawl results into MongoDB.

## Features

- Qt6 GUI with tabs for:
  - Graph view
  - Weibo list view
  - Single video player view
  - All pictures view
  - All videos view
  - Crawl health monitor view
  - Download manager view (task progress for videos/pictures)
  - Settings view (crawler, anti-crawl, logging, cookie editor)
  - Log panel view
- Interactive graph:
  - Node drag + curved edges
  - Right-click node menu to inspect followers/fans UID lists
  - Mouse wheel zoom and keyboard zoom shortcuts (`+`, `-`, `0`)
  - Layout algorithms: Random, Circular, Force Directed, Kamada-Kawai, Grid, Hierarchical
  - Theme switching
- Crawl options:
  - Crawl Weibo posts
  - Crawl fans
  - Crawl followers
  - Recursive crawl depth (`0..5`)
  - Breakpoint resume (queue state persisted to file)
  - Incremental crawl with early-stop on existing weibo IDs
- Configurable anti-crawl strategy:
  - Retry attempts/backoff
  - Request min interval + jitter
  - 429 cooldown window
- Configurable global log level (`trace`/`debug`/`info`/`warn`/`error`/`critical`/`off`)
- Media support:
  - Async image loading with cache
  - Video playback in Qt Multimedia
  - Save single video
  - Save all pictures for selected user
- MongoDB persistence (`weibo.user` collection)
  - Upsert-by-uid
  - Unique index on `uid`
  - Weibo deduplication on write

## Tech Stack

- C++17
- CMake
- Qt6 (`Widgets`, `Network`, `MultimediaWidgets`)
- cpp-httplib (OpenSSL)
- nlohmann/json
- spdlog + fmt
- mongo-cxx-driver (`mongocxx`, `bsoncxx`)
- OpenSSL

## Project Structure

```text
cpp-spider/
├── include/
│   ├── app_config.hpp
│   ├── graph_layout.hpp
│   ├── log_panel.hpp
│   ├── mainwindow.hpp
│   ├── qt_log_sink.hpp
│   ├── spider.hpp
│   ├── weibo.hpp
│   └── writer.hpp
├── src/
│   ├── app_config.cpp
│   ├── main.cpp
│   ├── mainwindow.cpp
│   ├── mainwindow_graph.cpp
│   ├── mainwindow_media.cpp
│   ├── mainwindow_spider.cpp
│   ├── mainwindow_ui.cpp
│   ├── log_panel.cpp
│   ├── spider.cpp
│   ├── weibo.cpp
│   └── writer.cpp
├── CMakeLists.txt
├── app_config.json
├── crawl_state.json (runtime-generated)
├── config.json
├── cookie.json
└── headers.json
```

## Architecture

```
MainWindow (GUI Thread)
    │
    ├─ creates ──► Spider (Worker Thread via QThread)
    │                  │
    │                  └─ callbacks ──► MongoWriter ──► MongoDB
    │
    └─ uses ──────► GraphLayout (static layout algorithms)
```

### Components

| Component | Files | Responsibility |
|-----------|-------|----------------|
| **MainWindow** | `mainwindow.hpp`, `mainwindow_*.cpp` | GUI orchestration, graph visualization, tabs (graph/weibo/video/pictures/videos/monitor/settings/logs) |
| **Spider** | `spider.hpp/cpp` | Crawling engine — HTTP requests, retry/anti-crawl, depth-based BFS crawl, breakpoint resume, metrics reporting |
| **MongoWriter** | `writer.hpp/cpp` | MongoDB connection and BSON document persistence |
| **AppConfig** | `app_config.hpp/cpp` | Centralized runtime configuration loading/saving from `app_config.json` |
| **LogPanel / QtLogSink** | `log_panel.*`, `qt_log_sink.hpp` | Structured GUI log panel and thread-safe `spdlog` to Qt bridge |
| **Weibo / User** | `weibo.hpp/cpp` | Data models for users and posts |
| **GraphLayout** | `graph_layout.hpp` | Header-only layout algorithms (Force-Directed, Circular, Grid, Hierarchical, Kamada-Kawai, Random) |

### Build Targets

- `libspider.so` — shared library containing Spider, MongoWriter, and data models
- `cpp-spider` — Qt6 GUI executable, links against `libspider`

### Threading Model

- **Main thread**: Qt event loop, rendering, user interaction
- **Worker thread**: `Spider::run()` executes HTTP requests and JSON parsing
- **Detached threads**: async image loading with cache
- **Thread communication**: `QMetaObject::invokeMethod` with `Qt::QueuedConnection`

### Data Flow

1. User enters target UID and crawl options in MainWindow
2. Spider runs in a worker thread, fetching data from Weibo Ajax endpoints
3. Callbacks push UI updates back to the main thread via queued invocations
4. MongoWriter persists crawled data to MongoDB (`weibo.user` collection)
5. GraphLayout positions nodes for the interactive relationship graph

## Prerequisites

- CMake 3.15+
- C++17 compiler
- Qt6 dev packages
- MongoDB running locally at `mongodb://0.0.0.0:27017`
- Install/findable CMake packages for:
  - `httplib`
  - `fmt`
  - `spdlog`
  - `nlohmann_json`
  - `bsoncxx`
  - `mongocxx`
  - `OpenSSL`

## Build & Run

Prerequisites:

- MongoDB running at `mongodb://0.0.0.0:27017`
- `cookie.json` with valid Weibo cookies in project root
- `headers.json` with valid Weibo session headers in project root

Dependencies:

- CMake 3.15+
- C++17 compiler
- Qt6 dev packages
- OpenSSL dev packages
- CMake packages for `httplib`, `fmt`, `spdlog`, `nlohmann_json`, `bsoncxx`, `mongocxx`

```bash
mkdir -p build
cd build
cmake ..
make -j
```

Helper scripts (from project root):

```bash
./scripts/build.sh   # configure + build
./scripts/test.sh    # build + run ctest
./scripts/start.sh   # run app (auto-build if missing)
```

Binary output:

```text
build/cpp-spider
build/libspider.so
```

From project root:

```bash
./build/cpp-spider
```

## Running Tests

This project uses Google Test for automated tests.
It also includes Qt UI automation tests for main window behavior.

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j
ctest --output-on-failure
```

If GTest is not available in your environment, CMake will skip test target setup.

## Configuration Files

### `app_config.json`

Primary runtime configuration. Includes:

- MongoDB settings (`mongo_url`, `mongo_db`, `mongo_collection`)
- File paths (`cookie_path`, `headers_path`, `config_path`, `crawl_state_path`)
- Crawl defaults (`default_uid`, `crawl_max_depth`)
- Retry + anti-crawl tuning (`retry_*`, `request_*`, `cooldown_429_ms`)
- Logging (`log_level`)

Example:

```json
{
  "mongo_url": "mongodb://0.0.0.0:27017",
  "mongo_db": "weibo",
  "mongo_collection": "user",
  "cookie_path": "/home/user/cpp-spider/cookie.json",
  "headers_path": "/home/user/cpp-spider/headers.json",
  "config_path": "/home/user/cpp-spider/config.json",
  "crawl_state_path": "/home/user/cpp-spider/crawl_state.json",
  "default_uid": 6126303533,
  "crawl_max_depth": 1,
  "retry_max_attempts": 5,
  "retry_base_delay_ms": 1000,
  "retry_max_delay_ms": 10000,
  "retry_backoff_factor": 2.0,
  "request_min_interval_ms": 800,
  "request_jitter_ms": 400,
  "cooldown_429_ms": 30000,
  "log_level": "info"
}
```

### `cookie.json`

JSON object of cookie key-values used for authenticated requests.

Example:

```json
{
  "SUB": "...",
  "SUBP": "...",
  "XSRF-TOKEN": "..."
}
```

### `headers.json`

JSON object of HTTP headers merged into the scraper client defaults.

Example:

```json
{
  "User-Agent": "Mozilla/5.0 ...",
  "Referer": "https://weibo.com/",
  "Accept": "application/json, text/plain, */*"
}
```

### `config.json`

UI persistence currently stores:

- `crawl_weibo` (bool)
- `target_uid` (string)
- `crawl_depth` (int)

Example:

```json
{
  "crawl_weibo": true,
  "target_uid": "6126303533",
  "crawl_depth": 1
}
```

### `crawl_state.json`

Runtime-generated breakpoint file for resume. Stores crawl queue cursor, visited set, and metrics snapshot for the active task.

## How It Works

1. `MainWindow` starts a worker `QThread`.
2. Worker creates `Spider(uid, app_config)` and sets callbacks (from `libspider.so`).
3. `Spider` runs depth-based BFS crawl (profile, fans/followers, weibos) from the target UID.
4. Request execution uses configurable retry/backoff and anti-crawl pacing.
5. Crawl queue state is persisted periodically to support resume after interruption.
6. UI updates are pushed back with `QMetaObject::invokeMethod` (queued/blocking queued where appropriate).
7. Crawled user data is upserted by `MongoWriter` into MongoDB.

## MongoDB Output

- Database: `weibo`
- Collection: `user`

Current stored document shape:

```json
{
  "uid": "1234567890",
  "username": "name",
  "weibos": [
    {
      "id": "post_id",
      "timestamp": "...",
      "text": "...",
      "pics": ["..."]
    }
  ]
}
```

Notes:

- `video_url` is persisted in MongoDB.
- Writes are incremental and de-duplicated by weibo id.

## Current Limitations

- Very large users can still stress UI rendering; weibo view uses paging to mitigate this.
- TLS verification is disabled in scraper HTTP client.
- No automated tests are included.

## Development Notes

- Logging uses `spdlog`.
- Graph layout algorithms are implemented in `include/graph_layout.hpp`.
- UI and crawler communicate with callbacks and queued Qt invocations for thread-safe updates.

## Troubleshooting

- App starts but fetch fails:
  - Verify `cookie.json` and `headers.json` are valid and not expired.
  - Confirm Weibo endpoints are reachable from your network.
- Mongo write fails:
  - Confirm MongoDB is running at `0.0.0.0:27017`.
  - Check driver installation and CMake package discovery.
- Media not loading:
  - Some URLs may require valid cookies or may expire quickly.
- HTTPS errors:
  - Ensure OpenSSL dev packages are installed and CMake finds them.

## License

No license file is currently included in this repository.
