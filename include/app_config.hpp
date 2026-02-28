#ifndef APP_CONFIG_HPP
#define APP_CONFIG_HPP

#include <cstdint>
#include <string>

struct AppConfig {
  // MongoDB
  std::string mongo_url = "mongodb://0.0.0.0:27017";
  std::string mongo_db = "weibo";
  std::string mongo_collection = "user";

  // File paths
  std::string cookie_path = "cookie.json";
  std::string headers_path = "headers.json";
  std::string config_path = "config.json";
  std::string crawl_state_path = "crawl_state.json";

  // Weibo API
  std::string weibo_host = "https://www.weibo.com";
  std::string image_host = "https://weibo.com";

  // Default target
  uint64_t default_uid = 6126303533;
  int crawl_max_depth = 1;

  // Retry strategy
  int retry_max_attempts = 5;
  int retry_base_delay_ms = 1000;
  int retry_max_delay_ms = 10000;
  double retry_backoff_factor = 2.0;

  // Anti-crawl stabilization
  int request_min_interval_ms = 800;
  int request_jitter_ms = 400;
  int cooldown_429_ms = 30000;

  // Logging
  std::string log_level = "info";

  // Load from JSON file; returns default config on failure
  static AppConfig load(const std::string &path = "app_config.json");

  // Save current config to JSON file
  void save(const std::string &path = "app_config.json") const;
};

#endif  // APP_CONFIG_HPP
