#include "spider.hpp"
#include "writer.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <iterator>
#include <fmt/core.h>
#include <fstream>
#include <httplib.h>
#include <memory>
#include <mongocxx/instance.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <set>
#include <sys/types.h>
#include <thread>
#include <utility>

using json = nlohmann::json;

namespace {
mongocxx::instance mongo_instance{};

json load_json_from_file(const std::string &path, const std::string &name) {
  spdlog::debug(fmt::format("loading {} from {}", name, path));

  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    const std::string msg = fmt::format("failed to open {} file: {}", name, path);
    spdlog::error(msg);
    throw std::runtime_error(msg);
  }

  const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  spdlog::debug(fmt::format("{} file size: {} bytes", name, content.size()));
  if (content.empty()) {
    const std::string msg = fmt::format("{} file is empty: {}", name, path);
    spdlog::error(msg);
    throw std::runtime_error(msg);
  }

  try {
    json parsed = json::parse(content);
    if (!parsed.is_object()) {
      spdlog::warn(fmt::format("{} json is not an object: {}", name, path));
    }
    return parsed;
  } catch (const std::exception &e) {
    const std::string preview = content.substr(0, std::min<size_t>(content.size(), 120));
    spdlog::error(fmt::format(
        "failed to parse {} json: {} (path={}, preview='{}')",
        name,
        e.what(),
        path,
        preview));
    throw;
  }
}
}

std::vector<Weibo> load_weibos_from_db(const AppConfig &config, uint64_t uid) {
  MongoWriter writer(config.mongo_url, config.mongo_db, config.mongo_collection);
  return writer.get_weibos(uid);
}

Spider::Spider(uint64_t uid, const AppConfig &config) {
  m_writer = std::make_unique<MongoWriter>(config.mongo_url, config.mongo_db, config.mongo_collection);
  m_visit_cnt = 0;
  m_crawlWeibo = true;
  m_crawlFans = true;
  m_crawlFollowers = true;
  m_max_depth = std::max(0, config.crawl_max_depth);
  m_running = false;
  m_state_path = config.crawl_state_path;
  m_users_processed = 0;
  m_users_failed = 0;
  m_requests_total = 0;
  m_requests_failed = 0;
  m_retries_total = 0;
  m_http_429_count = 0;
  m_current_uid = uid;
  m_queue_pending = 0;
  m_visited_total = 0;
  m_retry_max_attempts = std::max(1, config.retry_max_attempts);
  m_retry_base_delay_ms = std::max(0, config.retry_base_delay_ms);
  m_retry_max_delay_ms = std::max(m_retry_base_delay_ms, config.retry_max_delay_ms);
  m_retry_backoff_factor = std::max(1.0, config.retry_backoff_factor);
  m_request_min_interval_ms = std::max(0, config.request_min_interval_ms);
  m_request_jitter_ms = std::max(0, config.request_jitter_ms);
  m_cooldown_429_ms = std::max(0, config.cooldown_429_ms);
  m_next_request_time = std::chrono::steady_clock::now();
  m_rng = std::mt19937(std::random_device{}());
  m_self = User(uid, "", std::vector<User>());
  m_client = std::make_unique<httplib::Client>(config.weibo_host);
  httplib::Headers header;

  spdlog::info(fmt::format(
      "spider init: uid={}, weibo_host={}, cookie_path={}, headers_path={}",
      uid,
      config.weibo_host,
      config.cookie_path,
      config.headers_path));
  json json_cookie = load_json_from_file(config.cookie_path, "cookie");
  std::string cookie;
  for (json::iterator it = json_cookie.begin(); it != json_cookie.end();
       ++it) {
    if (!cookie.empty()) {
      cookie += "; ";
    }
    cookie += it.key() + "=" + it.value().get<std::string>();
  }
  header.insert(std::make_pair("Cookie", cookie));
  spdlog::debug(fmt::format("cookie header length: {}", cookie.size()));

  json json_headers = load_json_from_file(config.headers_path, "headers");
  int header_count = 0;
  for (json::iterator it = json_headers.begin(); it != json_headers.end();
       ++it) {
    header.insert(std::make_pair<std::string, std::string>(
        std::string(it.key()), it.value().get<std::string>()));
    header_count++;
  }
  spdlog::debug(fmt::format("default headers loaded: {}", header_count));

  m_client->set_default_headers(header);
  m_client->set_read_timeout(30, 0);
  m_client->set_write_timeout(30, 0);
  m_client->enable_server_hostname_verification(false);
  m_client->enable_server_certificate_verification(false);
  m_client->set_keep_alive(true);
  spdlog::info(fmt::format(
      "retry strategy: attempts={}, base={}ms, max={}ms, factor={}",
      m_retry_max_attempts,
      m_retry_base_delay_ms,
      m_retry_max_delay_ms,
      m_retry_backoff_factor));
  spdlog::info(fmt::format(
      "anti-crawl strategy: min_interval={}ms, jitter={}ms, cooldown_429={}ms",
      m_request_min_interval_ms,
      m_request_jitter_ms,
      m_cooldown_429_ms));
}

Spider::~Spider() = default;

void Spider::setUserCallback(UserCallback callback) {
  m_userCallback = std::move(callback);
}

void Spider::setCrawlWeibo(bool crawl) {
  m_crawlWeibo = crawl;
}

void Spider::setWeiboCallback(WeiboCallback callback) {
  m_weiboCallback = std::move(callback);
}

void Spider::setCrawlFans(bool crawl) {
  m_crawlFans = crawl;
}

void Spider::setCrawlFollowers(bool crawl) {
  m_crawlFollowers = crawl;
}

void Spider::setMaxDepth(int max_depth) {
  m_max_depth = std::max(0, max_depth);
}

void Spider::setMetricsCallback(MetricsCallback callback) {
  m_metricsCallback = std::move(callback);
}


void Spider::stop() {
  m_running = false;
}

bool Spider::is_retryable_result(const httplib::Result &result) const {
  if (!result) {
    return true;
  }
  if (result->status == 429) {
    return true;
  }
  return result->status >= 500;
}

int Spider::get_retry_delay_ms(int attempt) const {
  int64_t delay_ms = m_retry_base_delay_ms;
  for (int i = 1; i < attempt; ++i) {
    delay_ms = static_cast<int64_t>(delay_ms * m_retry_backoff_factor);
    if (delay_ms >= m_retry_max_delay_ms) {
      return m_retry_max_delay_ms;
    }
  }
  return static_cast<int>(std::min<int64_t>(delay_ms, m_retry_max_delay_ms));
}

int Spider::get_jitter_delay_ms() const {
  if (m_request_jitter_ms <= 0) {
    return 0;
  }
  std::uniform_int_distribution<int> jitter_dist(0, m_request_jitter_ms);
  return jitter_dist(m_rng);
}

void Spider::wait_for_request_slot() const {
  const int jitter_ms = get_jitter_delay_ms();
  const auto gap = std::chrono::milliseconds(m_request_min_interval_ms + jitter_ms);
  const auto now = std::chrono::steady_clock::now();
  auto wait_until = now;
  {
    std::lock_guard<std::mutex> lock(m_rate_limit_mutex);
    if (m_next_request_time > now) {
      wait_until = m_next_request_time;
    }
    m_next_request_time = std::max(m_next_request_time, now) + gap;
  }
  if (wait_until > now) {
    const auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        wait_until - now).count();
    spdlog::debug(fmt::format("request pacing sleep {}ms", wait_ms));
    std::this_thread::sleep_until(wait_until);
  }
}

void Spider::emit_metrics(bool force) {
  if (!m_metricsCallback) {
    return;
  }
  if (!force && (m_requests_total % 20 != 0)) {
    return;
  }
  m_metricsCallback(
      m_users_processed,
      m_users_failed,
      m_requests_total,
      m_requests_failed,
      m_retries_total,
      m_http_429_count,
      m_queue_pending,
      m_visited_total,
      m_current_uid);
}

bool Spider::load_crawl_state(std::vector<std::pair<uint64_t, int>> *queue,
                              size_t *cursor,
                              std::set<uint64_t> *visited) {
  if (m_state_path.empty() || !queue || !cursor || !visited) {
    return false;
  }
  std::ifstream ifs(m_state_path);
  if (!ifs.is_open()) {
    return false;
  }
  try {
    json j = json::parse(ifs);
    if (!j.contains("root_uid") || j["root_uid"].get<uint64_t>() != m_self.uid) {
      return false;
    }
    if (!j.contains("max_depth") || j["max_depth"].get<int>() != m_max_depth) {
      return false;
    }
    if (!j.contains("crawl_weibo") || j["crawl_weibo"].get<bool>() != m_crawlWeibo) {
      return false;
    }
    if (!j.contains("crawl_fans") || j["crawl_fans"].get<bool>() != m_crawlFans) {
      return false;
    }
    if (!j.contains("crawl_followers") || j["crawl_followers"].get<bool>() != m_crawlFollowers) {
      return false;
    }

    queue->clear();
    if (j.contains("queue") && j["queue"].is_array()) {
      for (const auto &item : j["queue"]) {
        if (!item.contains("uid") || !item.contains("depth")) {
          continue;
        }
        queue->emplace_back(item["uid"].get<uint64_t>(), item["depth"].get<int>());
      }
    }

    visited->clear();
    if (j.contains("visited") && j["visited"].is_array()) {
      for (const auto &uid_item : j["visited"]) {
        visited->insert(uid_item.get<uint64_t>());
      }
    }

    size_t saved_cursor = 0;
    if (j.contains("cursor")) {
      saved_cursor = j["cursor"].get<size_t>();
    }
    *cursor = std::min(saved_cursor, queue->size());

    if (j.contains("metrics") && j["metrics"].is_object()) {
      auto metrics = j["metrics"];
      m_users_processed = metrics.value("users_processed", static_cast<uint64_t>(0));
      m_users_failed = metrics.value("users_failed", static_cast<uint64_t>(0));
      m_requests_total = metrics.value("requests_total", static_cast<uint64_t>(0));
      m_requests_failed = metrics.value("requests_failed", static_cast<uint64_t>(0));
      m_retries_total = metrics.value("retries_total", static_cast<uint64_t>(0));
      m_http_429_count = metrics.value("http_429_count", static_cast<uint64_t>(0));
    }
    spdlog::info(fmt::format(
        "resume crawl from state: root_uid={}, cursor={}, queue={}, visited={}",
        m_self.uid,
        *cursor,
        queue->size(),
        visited->size()));
    return !queue->empty();
  } catch (const std::exception &e) {
    spdlog::warn(fmt::format("ignore invalid crawl state {}: {}", m_state_path, e.what()));
    return false;
  }
}

void Spider::save_crawl_state(const std::vector<std::pair<uint64_t, int>> &queue,
                              size_t cursor,
                              const std::set<uint64_t> &visited,
                              uint64_t current_uid) {
  if (m_state_path.empty()) {
    return;
  }
  try {
    json j;
    j["root_uid"] = m_self.uid;
    j["max_depth"] = m_max_depth;
    j["crawl_weibo"] = m_crawlWeibo;
    j["crawl_fans"] = m_crawlFans;
    j["crawl_followers"] = m_crawlFollowers;
    j["cursor"] = cursor;
    j["current_uid"] = current_uid;

    json queue_json = json::array();
    for (const auto &item : queue) {
      queue_json.push_back({{"uid", item.first}, {"depth", item.second}});
    }
    j["queue"] = std::move(queue_json);

    json visited_json = json::array();
    for (const auto uid : visited) {
      visited_json.push_back(uid);
    }
    j["visited"] = std::move(visited_json);

    j["metrics"] = {
        {"users_processed", m_users_processed},
        {"users_failed", m_users_failed},
        {"requests_total", m_requests_total},
        {"requests_failed", m_requests_failed},
        {"retries_total", m_retries_total},
        {"http_429_count", m_http_429_count},
    };

    std::ofstream ofs(m_state_path);
    ofs << j.dump(2) << std::endl;
  } catch (const std::exception &e) {
    spdlog::warn(fmt::format("save crawl state failed {}: {}", m_state_path, e.what()));
  }
}

void Spider::clear_crawl_state() {
  if (m_state_path.empty()) {
    return;
  }
  std::remove(m_state_path.c_str());
}

httplib::Result Spider::get_with_retry(const std::string &url,
                                       const std::string &request_name) {
  for (int attempt = 1; attempt <= m_retry_max_attempts && m_running; ++attempt) {
    m_requests_total++;
    wait_for_request_slot();
    auto result = m_client->Get(url);
    if (result && result->status >= 200 && result->status < 300) {
      if (attempt > 1) {
        m_retries_total += static_cast<uint64_t>(attempt - 1);
        spdlog::info(fmt::format(
            "{} succeeded on retry attempt {}/{}",
            request_name,
            attempt,
            m_retry_max_attempts));
      }
      emit_metrics(false);
      return result;
    }
    if (!is_retryable_result(result) || attempt == m_retry_max_attempts) {
      m_requests_failed++;
      if (attempt > 1) {
        m_retries_total += static_cast<uint64_t>(attempt - 1);
      }
      if (result) {
        if (result->status == 429) {
          m_http_429_count++;
        }
        spdlog::error(fmt::format(
            "{} failed after {}/{} attempts, status={}",
            request_name,
            attempt,
            m_retry_max_attempts,
            result->status));
      } else {
        spdlog::error(fmt::format(
            "{} failed after {}/{} attempts, error={}",
            request_name,
            attempt,
            m_retry_max_attempts,
            static_cast<int>(result.error())));
      }
      emit_metrics(false);
      return result;
    }

    const int delay_ms = get_retry_delay_ms(attempt);
    if (result) {
      if (result->status == 429 && m_cooldown_429_ms > 0) {
        m_http_429_count++;
        const auto cooldown_until = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(m_cooldown_429_ms);
        {
          std::lock_guard<std::mutex> lock(m_rate_limit_mutex);
          m_next_request_time = std::max(m_next_request_time, cooldown_until);
        }
        spdlog::warn(fmt::format(
            "{} got 429, applying cooldown {}ms",
            request_name,
            m_cooldown_429_ms));
      }
      spdlog::warn(fmt::format(
          "{} attempt {}/{} failed, status={}, retry in {}ms",
          request_name,
          attempt,
          m_retry_max_attempts,
          result->status,
          delay_ms));
    } else {
      spdlog::warn(fmt::format(
          "{} attempt {}/{} failed, error={}, retry in {}ms",
          request_name,
          attempt,
          m_retry_max_attempts,
          static_cast<int>(result.error()),
          delay_ms));
    }
    m_retries_total++;
    emit_metrics(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
  }
  return {};
}

void Spider::notifyUserFetched(uint64_t uid, const std::string& name,
                              const std::vector<uint64_t>& followers,
                              const std::vector<uint64_t>& fans) {
  if (m_userCallback) {
    m_userCallback(uid, name, followers, fans);
  }
}

User Spider::get_user(uint64_t uid,
                      bool get_follower,
                      std::vector<uint64_t> *follower_ids_out,
                      std::vector<uint64_t> *fan_ids_out) {
  if (!m_running) {
    return User(uid, "", {});
  }
  m_visit_cnt++;
  if (m_visit_cnt % 80 == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
  if (!m_running) {
    return User(uid, "", {});
  }
  const std::string url = fmt::format("/ajax/profile/info?uid={}", uid);
  spdlog::info(url);
  try {
    httplib::Result resp = get_with_retry(url, fmt::format("get_user uid={}", uid));
    if (!resp) {
      return User(uid, "", {});
    }
    auto json_resp = json::parse(resp->body);
    if (spdlog::default_logger_raw() && spdlog::default_logger()->should_log(spdlog::level::debug)) {
      std::string payload = json_resp.dump();
      if (payload.size() > 400) {
        payload = payload.substr(0, 400) + "...";
      }
      spdlog::debug(fmt::format("profile payload uid={} {}", uid, payload));
    }
    if (!json_resp.contains("ok") || json_resp["ok"].get<int>() != 1) {
      return User(uid, "", {});
    }
    auto user = json_resp["data"]["user"];
    auto name = user["screen_name"].get<std::string>();
    spdlog::info(fmt::format("screen name:{}", name));
    spdlog::info(fmt::format("uid: {}, user name {}", uid, name));

    std::vector<uint64_t> follower_ids;
    std::vector<uint64_t> fan_ids;
    if (get_follower) {
      User user(uid, name, {});
      if (m_crawlFollowers) {
        user.followers = get_self_follower(uid);
        for (const auto& f : user.followers) {
          follower_ids.push_back(f.uid);
        }
      }
      if (m_crawlFans) {
        auto fans = get_other_follower(uid);
        for (const auto& f : fans) {
          fan_ids.push_back(f.uid);
        }
      }
      if (follower_ids_out) {
        *follower_ids_out = follower_ids;
      }
      if (fan_ids_out) {
        *fan_ids_out = fan_ids;
      }
      notifyUserFetched(uid, name, follower_ids, fan_ids);
      return user;
    } else {
      notifyUserFetched(uid, name, {}, {});
      User user(uid, name, {});
      return user;
    }
  } catch (const std::exception &e) {
    spdlog::error(e.what());
  }
  return User(uid, "", {});
}

std::vector<User> Spider::get_self_follower(uint64_t uid) {
  std::vector<uint64_t> ids;
  const std::string url =
      fmt::format("/ajax/friendships/friends?uid={}&relate=fans&count=20&fansSortType=fansCount",
                  uid);
  auto result = get_with_retry(url, "get_self_follower");
  if (!result) {
    spdlog::error("HTTP request failed for self follower");
    return {};
  }
  auto resp = json::parse(result->body);
  std::vector<json::basic_json::object_t> users = resp["users"];
  spdlog::info(fmt::format("self follower size:{}", users.size()));
  for (auto &item : users) {
    ids.push_back(item["id"].get<uint64_t>());
  }
  auto ret = batch_get_user(ids);
  return ret;
}

std::vector<User> Spider::get_other_follower(uint64_t uid) {
  spdlog::info(fmt::format("start to get other follower, uid: {}", uid));
  int page_cnt = 1;
  std::vector<uint64_t> ids;
  while (m_running) {
    const std::string url = fmt::format(
        "/ajax/friendships/"
        "friends?relate=fans&page={}&uid={}&type=all&newFollowerCount=0",
        page_cnt, uid);
    auto result = get_with_retry(
        url,
        fmt::format("get_other_follower uid={} page={}", uid, page_cnt));
    if (!result) {
      spdlog::error("HTTP request failed for other follower");
      break;
    }
    auto resp = json::parse(result->body);
    int total_cnt = resp["display_total_number"].get<uint>();
    std::vector<json::basic_json::object_t> users = resp["users"];
    for (auto &user : users) {
      ids.push_back(user["id"].get<uint64_t>());
    }
    if (ids.size() >= total_cnt || users.empty()) {
      break;
    } else {
      page_cnt += 1;
    }
    if (page_cnt % 20 == 0) {
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    spdlog::info(
        fmt::format("total {} followers, current {}", total_cnt, ids.size()));
  }
  spdlog::info(fmt::format("success to get {} followers", ids.size()));
  auto fans = batch_get_user(ids);
  for (const auto &fan : fans) {
    spdlog::info(
        fmt::format("get follower id {}, username: {}", fan.uid, fan.username));
  }
  return fans;
}

void Spider::run() {
  m_running = true;
  spdlog::info(fmt::format(
      "spider run started, root uid={}, max_depth={}",
      m_self.uid,
      m_max_depth));

  std::set<uint64_t> visited;
  std::vector<std::pair<uint64_t, int>> queue;
  size_t cursor = 0;

  if (!load_crawl_state(&queue, &cursor, &visited)) {
    queue.clear();
    queue.emplace_back(m_self.uid, 0);
    cursor = 0;
  }

  m_queue_pending = queue.size() > cursor ? queue.size() - cursor : 0;
  m_visited_total = visited.size();
  emit_metrics(true);
  save_crawl_state(queue, cursor, visited, m_self.uid);

  while (m_running && cursor < queue.size()) {
    const auto [uid, depth] = queue[cursor++];
    if (visited.count(uid)) {
      m_queue_pending = queue.size() > cursor ? queue.size() - cursor : 0;
      m_visited_total = visited.size();
      emit_metrics(false);
      continue;
    }
    visited.insert(uid);
    m_current_uid = uid;

    const bool need_relations =
        depth < m_max_depth && (m_crawlFollowers || m_crawlFans);
    std::vector<uint64_t> follower_ids;
    std::vector<uint64_t> fan_ids;
    User user = get_user(uid, need_relations, &follower_ids, &fan_ids);
    if (user.username.empty()) {
      m_users_failed++;
      m_queue_pending = queue.size() > cursor ? queue.size() - cursor : 0;
      m_visited_total = visited.size();
      emit_metrics(true);
      save_crawl_state(queue, cursor, visited, uid);
      continue;
    }

    if (m_crawlWeibo) {
      user.set_weibo(get_weibo(user));
      if (m_weiboCallback) {
        m_weiboCallback(user.uid, user.weibo);
      }
    }

    m_writer->write_one(user);
    m_users_processed++;
    spdlog::info("write uid: {} to mongodb!", user.uid);

    if (need_relations) {
      for (const auto id : follower_ids) {
        if (!visited.count(id)) {
          queue.emplace_back(id, depth + 1);
        }
      }
      for (const auto id : fan_ids) {
        if (!visited.count(id)) {
          queue.emplace_back(id, depth + 1);
        }
      }
    }

    m_queue_pending = queue.size() > cursor ? queue.size() - cursor : 0;
    m_visited_total = visited.size();
    emit_metrics(true);
    save_crawl_state(queue, cursor, visited, uid);
  }

  if (cursor >= queue.size()) {
    clear_crawl_state();
  }
  m_queue_pending = 0;
  m_visited_total = visited.size();
  emit_metrics(true);
  spdlog::info(fmt::format("spider run finished, root uid={}", m_self.uid));
}

std::vector<User> Spider::batch_get_user(const std::vector<uint64_t> &ids) {
  std::vector<User> ret;
  for (const auto &id : ids) {
    if (!m_running) break;
    ret.push_back(get_user(id));
  }
  return ret;
}

std::vector<Weibo> Spider::get_weibo(const User &user) {
  int page_cnt = 1;
  std::vector<Weibo> weibos;

  // Load existing weibo IDs to skip already-stored posts
  std::set<uint64_t> existing_ids = m_writer->get_stored_weibo_ids(user.uid);
  spdlog::info(fmt::format(
      "{} existing weibos in db for uid {}",
      existing_ids.size(), user.uid));

  bool hit_existing = false;
  while (m_running && !hit_existing) {
    const std::string url = fmt::format(
        "/ajax/statuses/mymblog?uid={}&page={}&", user.uid, page_cnt);
    auto result = get_with_retry(
        url,
        fmt::format("get_weibo uid={} page={}", user.uid, page_cnt));
    if (!result) {
      spdlog::error("HTTP request failed for weibo");
      break;
    }
    auto resp = json::parse(result->body);
    auto data = resp["data"];
    std::vector<json::basic_json::object_t> items = data["list"];
    if (items.empty()) {
      break;
    }
    for (auto &item : items) {
       std::string tm = item["created_at"].get<std::string>();
       std::string text = item["text"].get<std::string>();
       uint64_t id = item["id"].get<uint64_t>();

       // Stop when we hit an already-stored weibo
       if (existing_ids.count(id)) {
         spdlog::info(fmt::format(
             "hit existing weibo id , stopping", id));
         hit_existing = true;
         break;
       }

       std::vector<std::string> urls;
       if (item.count("pic_infos")) {
         const auto& pic_infos = item["pic_infos"];
         for (auto& [key, value] : pic_infos.items()) {
           if (!value.contains("large")) {
             continue;
           }
           std::string url = value["large"]["url"].get<std::string>();
           urls.push_back(url);
         }
       }
       std::string video_url;
       if (item.count("page_info") && item["page_info"].count("media_info") && 
           item["page_info"]["media_info"].count("stream_url")) {
         video_url = item["page_info"]["media_info"]["stream_url"].get<std::string>();
         if (video_url.find("http") != 0) {
           video_url = "";
         }
       }
       Weibo weibo(text, tm, id, urls, video_url);
       weibos.push_back(weibo);
       spdlog::info(fmt::format(
           "crawling uid {} weibo #{} (id={})",
           user.uid,
           weibos.size(),
           id));
     }
    page_cnt += 1;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  spdlog::info(fmt::format(
      "weibo crawl done: {} new weibos for uid {}",
      weibos.size(), user.uid));
  return weibos;
}
