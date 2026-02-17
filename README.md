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
- Interactive graph:
  - Node drag + curved edges
  - Mouse wheel zoom and keyboard zoom shortcuts (`+`, `-`, `0`)
  - Layout algorithms: Random, Circular, Force Directed, Kamada-Kawai, Grid, Hierarchical
  - Theme switching
- Crawl options:
  - Crawl Weibo posts
  - Crawl fans
  - Crawl followers
- Media support:
  - Async image loading with cache
  - Video playback in Qt Multimedia
  - Save single video
  - Save all pictures for selected user
- MongoDB persistence (`weibo.user` collection)

## Tech Stack

- C++17
- CMake
- Qt6 (`Widgets`, `Network`, `MultimediaWidgets`)
- cpp-httplib
- nlohmann/json
- spdlog + fmt
- mongo-cxx-driver (`mongocxx`, `bsoncxx`)

## Project Structure

```text
cpp-spider/
├── include/
│   ├── graph_layout.hpp
│   ├── mainwindow.hpp
│   ├── spider.hpp
│   ├── weibo.hpp
│   └── writer.hpp
├── src/
│   ├── main.cpp
│   ├── mainwindow.cpp
│   ├── spider.cpp
│   ├── weibo.cpp
│   └── writer.cpp
├── CMakeLists.txt
├── config.json
├── cookie.json
└── headers.json
```

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

## Build & Run

Prerequisites:

- MongoDB running at `mongodb://0.0.0.0:27017`
- `cookie.json` with valid Weibo cookies in project root
- `headers.json` with valid Weibo session headers in project root

Dependencies:

- CMake 3.15+
- C++17 compiler
- Qt6 dev packages
- CMake packages for `httplib`, `fmt`, `spdlog`, `nlohmann_json`, `bsoncxx`, `mongocxx`

```bash
mkdir -p build
cd build
cmake ..
make -j
```

Binary output:

```text
build/cpp-spider
```

From project root:

```bash
./build/cpp-spider
```

## Configuration Files

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

Example:

```json
{
  "crawl_weibo": true,
  "target_uid": "6126303533"
}
```

## How It Works

1. `MainWindow` starts a worker `QThread`.
2. Worker creates `Spider(uid)` and sets callbacks.
3. `Spider` fetches profile/fans/followers/posts from Weibo Ajax endpoints.
4. UI updates are pushed back with `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.
5. Crawled user data is written by `MongoWriter` into MongoDB.

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

- `video_url` is collected in memory for UI but is not persisted by `MongoWriter` currently.
- Follower IDs are built in `MongoWriter::write_one` but are not appended into the final BSON document.

## Current Limitations

- `Spider::run()` currently writes only the target/root user to MongoDB. The recursive follower crawl/write loop is present but commented out.
- `cookie.json` and `headers.json` are loaded via hard-coded absolute paths in code:
  - `/home/gugugu/Repo/cpp-spider/cookie.json`
  - `/home/gugugu/Repo/cpp-spider/headers.json`
  This makes runtime path-sensitive across machines.
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

## License

No license file is currently included in this repository.
