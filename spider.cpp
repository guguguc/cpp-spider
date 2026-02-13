#include "spider.hpp"
#include <chrono>
#include <cstdint>
#include <fmt/core.h>
#include <fstream>
#include <httplib.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <utility>

using json = nlohmann::json;

Spider::Spider(uint64_t uid) : m_writer("mongodb://0.0.0.0:27017") {
  m_visit_cnt = 0;
  m_self = User(uid, "", std::vector<User>());
  m_client = std::make_unique<httplib::Client>("https://weibo.com");
  httplib::Headers header;
  std::ifstream ifs("/home/gugugu/Repo/cpp-spider/build/headers.json");
  json json_headers = json::parse(ifs);
  for (json::iterator it = json_headers.begin(); it != json_headers.end();
       ++it) {
    header.insert(std::make_pair<std::string, std::string>(
        std::string(it.key()), it.value().get<std::string>()));
  }
  m_client->set_default_headers(header);
  m_client->set_read_timeout(30, 0);
  m_client->set_write_timeout(30, 0);
  m_client->enable_server_hostname_verification(false);
  m_client->enable_server_certificate_verification(false);
  m_client->set_keep_alive(true);
}

User Spider::get_user(uint64_t uid, bool get_follower) {
  m_visit_cnt++;
  if (m_visit_cnt % 100 == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
task:
  const std::string url = fmt::format("/ajax/profile/info?uid={}", uid);
  try {
    httplib::Result resp = m_client->Get(url);
    auto json_resp = json::parse(resp->body);
    if (!json_resp.contains("ok")) {
      return User(uid, "", {});
    }
    auto user = json_resp["data"]["user"];
    auto name = user["screen_name"].get<std::string>();
    spdlog::info(fmt::format("uid: {}, user name {}", uid, name));
    if (get_follower) {
      User user(uid, name, {});
      user.followers = get_self_follower();
      return user;
    } else {
      User user(uid, name, {});
      return user;
    }
  } catch (const std::exception &e) {
    spdlog::error(e.what());
    std::this_thread::sleep_for(std::chrono::seconds(10));
    goto task;
  }
}

std::vector<User> Spider::get_self_follower() {
  std::vector<uint64_t> ids;
  const std::string url =
      fmt::format("https://weibo.com/ajax/friendships/"
                  "friends?uid={}&relate=fans&count=20&fansSortType=fansCount",
                  m_self.uid);
  auto resp = json::parse(m_client->Get(url)->body);
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
  while (1) {
    const std::string url = fmt::format(
        "/ajax/friendships/"
        "friends?relate=fans&page={}&uid={}&type=all&newFollowerCount=0",
        page_cnt, uid);
    auto resp = json::parse(m_client->Get(url)->body);
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
  User user = get_user(m_self.uid, true);
  spdlog::info("get user");
  user.set_weibo(get_weibo(user));
  m_writer.write_one(user);
  spdlog::info("write uid: {} to mongodb!", user.uid);
  for (auto &item : user.followers) {
    auto followers = get_other_follower(item.uid);
    item.followers = followers;
    item.set_weibo(get_weibo(item));
    m_writer.write_one(item);
    spdlog::info("write uid: {} to mongodb!", item.uid);
  }
}

std::vector<User> Spider::batch_get_user(const std::vector<uint64_t> &ids) {
  std::vector<User> ret;
  for (const auto &id : ids) {
    ret.push_back(get_user(id));
  }
  return ret;
}

std::vector<Weibo> Spider::get_weibo(const User &user) {
  int page_cnt = 1;
  std::vector<Weibo> weibos;
  while (1) {
    const std::string url = fmt::format(
        "/ajax/statuses/mymblog?uid={}&page={}&", user.uid, page_cnt);
    auto resp = json::parse(m_client->Get(url)->body);
    auto data = resp["data"];
    std::vector<json::basic_json::object_t> items = data["list"];
    if (items.empty()) {
      break;
    }
    for (auto &item : items) {
      std::string tm = item["created_at"].get<std::string>();
      std::string text = item["text"].get<std::string>();
      uint64_t id = item["id"].get<uint64_t>();
      std::vector<std::string> urls;
      if (item.count("pic_infos")) {
        item = item["pic_infos"];
        for (auto it = item.begin(); it != item.end(); ++it) {
          if (!it->second.contains("large")) {
            continue;
          }
          std::string url = it->second["large"]["url"].get<std::string>();
          urls.push_back(url);
        }
      }
      Weibo weibo(text, tm, id, urls);
      spdlog::info(weibo.dump());
      weibos.push_back(weibo);
    }
    page_cnt += 1;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return weibos;
}

