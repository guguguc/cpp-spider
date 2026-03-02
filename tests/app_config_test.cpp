#include "app_config.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path unique_temp_path(const std::string &suffix) {
  const auto base = std::filesystem::temp_directory_path();
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto unique = std::filesystem::path("cpp_spider_test_" + std::to_string(stamp) + "_" + suffix);
  return base / unique;
}

}

TEST(AppConfigTest, LoadMissingFileReturnsDefaults) {
  const auto missing = unique_temp_path("missing_config.json");
  std::filesystem::remove(missing);

  const AppConfig cfg = AppConfig::load(missing.string());

  EXPECT_EQ(cfg.mongo_url, "mongodb://0.0.0.0:27017");
  EXPECT_EQ(cfg.mongo_db, "weibo");
  EXPECT_EQ(cfg.mongo_collection, "user");
  EXPECT_EQ(cfg.retry_max_attempts, 5);
  EXPECT_EQ(cfg.request_min_interval_ms, 800);
  EXPECT_EQ(cfg.log_level, "info");
}

TEST(AppConfigTest, SaveAndLoadRoundTripPersistsFields) {
  const auto path = unique_temp_path("roundtrip_config.json");

  AppConfig original;
  original.mongo_url = "mongodb://127.0.0.1:27017";
  original.mongo_db = "test_db";
  original.mongo_collection = "test_col";
  original.cookie_path = "cookie_test.json";
  original.headers_path = "headers_test.json";
  original.config_path = "config_test.json";
  original.crawl_state_path = "crawl_state_test.json";
  original.weibo_host = "https://example.com";
  original.image_host = "https://img.example.com";
  original.default_uid = 123456789;
  original.crawl_max_depth = 3;
  original.retry_max_attempts = 9;
  original.retry_base_delay_ms = 1500;
  original.retry_max_delay_ms = 12000;
  original.retry_backoff_factor = 2.5;
  original.request_min_interval_ms = 700;
  original.request_jitter_ms = 350;
  original.cooldown_429_ms = 45000;
  original.request_profile = "aggressive";
  original.log_level = "debug";

  original.save(path.string());
  const AppConfig loaded = AppConfig::load(path.string());

  EXPECT_EQ(loaded.mongo_url, original.mongo_url);
  EXPECT_EQ(loaded.mongo_db, original.mongo_db);
  EXPECT_EQ(loaded.mongo_collection, original.mongo_collection);
  EXPECT_EQ(loaded.cookie_path, original.cookie_path);
  EXPECT_EQ(loaded.headers_path, original.headers_path);
  EXPECT_EQ(loaded.config_path, original.config_path);
  EXPECT_EQ(loaded.crawl_state_path, original.crawl_state_path);
  EXPECT_EQ(loaded.weibo_host, original.weibo_host);
  EXPECT_EQ(loaded.image_host, original.image_host);
  EXPECT_EQ(loaded.default_uid, original.default_uid);
  EXPECT_EQ(loaded.crawl_max_depth, original.crawl_max_depth);
  EXPECT_EQ(loaded.retry_max_attempts, original.retry_max_attempts);
  EXPECT_EQ(loaded.retry_base_delay_ms, original.retry_base_delay_ms);
  EXPECT_EQ(loaded.retry_max_delay_ms, original.retry_max_delay_ms);
  EXPECT_DOUBLE_EQ(loaded.retry_backoff_factor, original.retry_backoff_factor);
  EXPECT_EQ(loaded.request_min_interval_ms, original.request_min_interval_ms);
  EXPECT_EQ(loaded.request_jitter_ms, original.request_jitter_ms);
  EXPECT_EQ(loaded.cooldown_429_ms, original.cooldown_429_ms);
  EXPECT_EQ(loaded.request_profile, original.request_profile);
  EXPECT_EQ(loaded.log_level, original.log_level);

  std::filesystem::remove(path);
}

TEST(AppConfigTest, LoadInvalidJsonFallsBackToDefaults) {
  const auto path = unique_temp_path("invalid_config.json");
  {
    std::ofstream ofs(path);
    ofs << "{ invalid json";
  }

  const AppConfig cfg = AppConfig::load(path.string());

  EXPECT_EQ(cfg.mongo_db, "weibo");
  EXPECT_EQ(cfg.retry_max_attempts, 5);
  EXPECT_EQ(cfg.request_profile, "balanced");

  std::filesystem::remove(path);
}

TEST(AppConfigTest, PartialConfigOnlyOverridesSpecifiedFields) {
  const auto path = unique_temp_path("partial_config.json");
  {
    std::ofstream ofs(path);
    ofs << R"({
  "mongo_db": "partial_db",
  "retry_max_attempts": 11,
  "log_level": "warn"
})";
  }

  const AppConfig cfg = AppConfig::load(path.string());

  EXPECT_EQ(cfg.mongo_db, "partial_db");
  EXPECT_EQ(cfg.retry_max_attempts, 11);
  EXPECT_EQ(cfg.log_level, "warn");
  EXPECT_EQ(cfg.mongo_collection, "user");
  EXPECT_EQ(cfg.request_profile, "balanced");

  std::filesystem::remove(path);
}
