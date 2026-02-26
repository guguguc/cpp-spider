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
    if (j.contains("weibo_host"))       cfg.weibo_host = j["weibo_host"].get<std::string>();
    if (j.contains("image_host"))       cfg.image_host = j["image_host"].get<std::string>();
    if (j.contains("default_uid"))      cfg.default_uid = j["default_uid"].get<uint64_t>();

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
    j["weibo_host"] = weibo_host;
    j["image_host"] = image_host;
    j["default_uid"] = default_uid;

    std::ofstream ofs(path);
    ofs << j.dump(2) << std::endl;
  } catch (const std::exception &e) {
    spdlog::error(fmt::format("failed to save config: {}", e.what()));
  }
}
