find_package(GTest QUIET)

if(NOT GTest_FOUND)
  message(WARNING "GTest not found. Skipping tests target setup.")
  return()
endif()

add_executable(spider_tests
  app_config_test.cpp
  graph_layout_test.cpp
  weibo_test.cpp
)

target_link_libraries(spider_tests PRIVATE
  GTest::gtest_main
  Qt6::Widgets
  spider
)

include(GoogleTest)
gtest_discover_tests(spider_tests)

add_executable(ui_mainwindow_test
  ui_mainwindow_test.cpp
  ../src/mainwindow.cpp
  ../src/mainwindow_ui.cpp
  ../src/mainwindow_graph.cpp
  ../src/mainwindow_spider.cpp
  ../src/mainwindow_media.cpp
  ../src/log_panel.cpp
  ../include/mainwindow.hpp
  ../include/log_panel.hpp
  ../include/qt_log_sink.hpp
)

target_include_directories(ui_mainwindow_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(ui_mainwindow_test PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_link_libraries(ui_mainwindow_test PRIVATE
  Qt6::Test
  Qt6::Widgets
  Qt6::Network
  Qt6::MultimediaWidgets
  OpenSSL::SSL
  OpenSSL::Crypto
  fmt::fmt
  spdlog::spdlog_header_only
  httplib::httplib
  spider
)

add_test(NAME ui_mainwindow_test COMMAND ui_mainwindow_test -platform offscreen)
