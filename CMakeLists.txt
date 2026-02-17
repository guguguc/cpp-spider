cmake_minimum_required(VERSION 3.15)

project(cpp-spider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network MultimediaWidgets)
find_package(OpenSSL REQUIRED)

find_package(spdlog)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json)
find_package(bsoncxx REQUIRED)
find_package(mongocxx REQUIRED)

add_library(spider SHARED
  src/spider.cpp
  src/weibo.cpp
  src/writer.cpp
  include/spider.hpp
  include/weibo.hpp
  include/writer.hpp
)

target_include_directories(spider PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(spider PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(spider PRIVATE OpenSSL::SSL OpenSSL::Crypto spdlog::spdlog_header_only httplib::httplib fmt::fmt nlohmann_json::nlohmann_json mongo::mongocxx_shared)

add_executable(cpp-spider
  src/main.cpp
  src/mainwindow.cpp
  include/mainwindow.hpp
  include/graph_layout.hpp
)

target_include_directories(cpp-spider PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_definitions(cpp-spider PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(cpp-spider PRIVATE Qt6::Widgets Qt6::Network Qt6::MultimediaWidgets OpenSSL::SSL OpenSSL::Crypto fmt::fmt spider)
