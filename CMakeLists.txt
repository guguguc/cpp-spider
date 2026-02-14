cmake_minimum_required(VERSION 3.15)

project(cpp-spider)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network MultimediaWidgets)

find_package(spdlog)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json)
find_package(bsoncxx REQUIRED)
find_package(mongocxx REQUIRED)

add_executable(cpp-spider main.cpp mainwindow.cpp spider.cpp weibo.cpp writer.cpp)
target_link_libraries(cpp-spider PRIVATE Qt6::Widgets Qt6::Network Qt6::MultimediaWidgets spdlog::spdlog_header_only httplib::httplib fmt::fmt nlohmann_json::nlohmann_json mongo::mongocxx_shared)
