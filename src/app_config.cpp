#include "app_config.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <fmt/core.h>

using json = nlohmann::json;

AppConfig AppConfig::load(const std::string &path) {
  AppConfig cfg;
  try {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      spdlog::warn(fmt::format("config file not found: {}, using defaults", path));
      return cfg;
    }
    json j = json::parse(ifs);

    if (j.contains("mongo_url"))        cfg.mongo_url = j["mongo_url"].get<std::string>();
    if (j.contains("mongo_db"))         cfg.mongo_db = j["mongo_db"].get<std::string>();
    if (j.contains("mongo_collection")) cfg.mongo_collection = j["mongo_collection"].get<std::string>();
    if (j.contains("cookie_path"))      cfg.cookie_path = j["cookie_path"].get<std::string>();
    if (j.contains("headers_path"))     cfg.headers_path = j["headers_path"].get<std::string>();
    if (j.contains("config_path"))      cfg.config_path = j["config_path"].get<std::string>();
    if (j.contains("crawl_state_path")) cfg.crawl_state_path = j["crawl_state_path"].get<std::string>();
    if (j.contains("weibo_host"))       cfg.weibo_host = j["weibo_host"].get<std::string>();
    if (j.contains("image_host"))       cfg.image_host = j["image_host"].get<std::string>();
    if (j.contains("default_uid"))      cfg.default_uid = j["default_uid"].get<uint64_t>();
    if (j.contains("crawl_max_depth"))  cfg.crawl_max_depth = j["crawl_max_depth"].get<int>();
    if (j.contains("retry_max_attempts")) cfg.retry_max_attempts = j["retry_max_attempts"].get<int>();
    if (j.contains("retry_base_delay_ms")) cfg.retry_base_delay_ms = j["retry_base_delay_ms"].get<int>();
    if (j.contains("retry_max_delay_ms")) cfg.retry_max_delay_ms = j["retry_max_delay_ms"].get<int>();
    if (j.contains("retry_backoff_factor")) cfg.retry_backoff_factor = j["retry_backoff_factor"].get<double>();
    if (j.contains("request_min_interval_ms")) cfg.request_min_interval_ms = j["request_min_interval_ms"].get<int>();
    if (j.contains("request_jitter_ms")) cfg.request_jitter_ms = j["request_jitter_ms"].get<int>();
    if (j.contains("cooldown_429_ms")) cfg.cooldown_429_ms = j["cooldown_429_ms"].get<int>();
    if (j.contains("log_level")) cfg.log_level = j["log_level"].get<std::string>();

    spdlog::info(fmt::format("loaded config from {}", path));
  } catch (const std::exception &e) {
    spdlog::error(fmt::format("failed to load config: {}", e.what()));
  }
  return cfg;
}

void AppConfig::save(const std::string &path) const {
  try {
    json j;
    j["mongo_url"] = mongo_url;
    j["mongo_db"] = mongo_db;
    j["mongo_collection"] = mongo_collection;
    j["cookie_path"] = cookie_path;
    j["headers_path"] = headers_path;
    j["config_path"] = config_path;
    j["crawl_state_path"] = crawl_state_path;
    j["weibo_host"] = weibo_host;
    j["image_host"] = image_host;
    j["default_uid"] = default_uid;
    j["crawl_max_depth"] = crawl_max_depth;
    j["retry_max_attempts"] = retry_max_attempts;
    j["retry_base_delay_ms"] = retry_base_delay_ms;
    j["retry_max_delay_ms"] = retry_max_delay_ms;
    j["retry_backoff_factor"] = retry_backoff_factor;
    j["request_min_interval_ms"] = request_min_interval_ms;
    j["request_jitter_ms"] = request_jitter_ms;
    j["cooldown_429_ms"] = cooldown_429_ms;
    j["log_level"] = log_level;

    std::ofstream ofs(path);
    ofs << j.dump(2) << std::endl;
  } catch (const std::exception &e) {
    spdlog::error(fmt::format("failed to save config: {}", e.what()));
  }
}
