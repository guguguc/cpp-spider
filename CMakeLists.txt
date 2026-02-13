cmake_minimum_required(VERSION 3.15)

project(cpp-spider)

find_package(spdlog)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json)
find_package(bsoncxx REQUIRED)
find_package(mongocxx REQUIRED)

add_executable(cpp-spider main.cpp spider.cpp weibo.cpp writer.cpp)
target_link_libraries(cpp-spider PRIVATE spdlog::spdlog_header_only httplib::httplib fmt::fmt nlohmann_json::nlohmann_json mongo::mongocxx_shared)
