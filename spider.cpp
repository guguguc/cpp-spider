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
  m_crawlWeibo = true;
  m_crawlFans = true;
  m_crawlFollowers = true;
  m_running = false;
  m_self = User(uid, "", std::vector<User>());
  m_client = std::make_unique<httplib::Client>("https://www.weibo.com");
  httplib::Headers header;

  std::ifstream cookie_ifs("/home/gugugu/Repo/cpp-spider/cookie.json");
  json json_cookie = json::parse(cookie_ifs);
  std::string cookie;
  for (json::iterator it = json_cookie.begin(); it != json_cookie.end();
       ++it) {
    if (!cookie.empty()) {
      cookie += "; ";
    }
    cookie += it.key() + "=" + it.value().get<std::string>();
  }
  header.insert(std::make_pair("Cookie", cookie));

  std::ifstream headers_ifs("/home/gugugu/Repo/cpp-spider/headers.json");
  json json_headers = json::parse(headers_ifs);
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

void Spider::stop() {
  m_running = false;
}

void Spider::notifyUserFetched(uint64_t uid, const std::string& name,
                              const std::vector<uint64_t>& followers,
                              const std::vector<uint64_t>& fans) {
  if (m_userCallback) {
    m_userCallback(uid, name, followers, fans);
  }
}

User Spider::get_user(uint64_t uid, bool get_follower) {
  if (!m_running) {
    return User(uid, "", {});
  }
  m_visit_cnt++;
  if (m_visit_cnt % 80 == 0) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }
task:
  if (!m_running) {
    return User(uid, "", {});
  }
  const std::string url = fmt::format("/ajax/profile/info?uid={}", uid);
  spdlog::info(url);
  try {
    httplib::Result resp = m_client->Get(url);
    auto json_resp = json::parse(resp->body);
    spdlog::info(json_resp.dump());
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
        user.followers = get_self_follower();
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
      notifyUserFetched(uid, name, follower_ids, fan_ids);
      return user;
    } else {
      notifyUserFetched(uid, name, {}, {});
      User user(uid, name, {});
      return user;
    }
  } catch (const std::exception &e) {
    spdlog::error(e.what());
    if (m_running) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
      goto task;
    }
  }
  return User(uid, "", {});
}

std::vector<User> Spider::get_self_follower() {
  std::vector<uint64_t> ids;
  const std::string url =
      fmt::format("/ajax/friendships/friends?uid={}&relate=fans&count=20&fansSortType=fansCount",
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
  while (m_running) {
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
  m_running = true;
  User user = get_user(m_self.uid, true);
  spdlog::info("get user");
  if (!m_running) return;
  if (m_crawlWeibo) {
    user.set_weibo(get_weibo(user));
    if (m_weiboCallback) {
      m_weiboCallback(user.uid, user.weibo);
    }
  }
  m_writer.write_one(user);
  // spdlog::info("write uid: {} to mongodb!", user.uid);
  // for (auto &item : user.followers) {
  //   if (!m_running) return;
  //   auto followers = get_other_follower(item.uid);
  //   item.followers = followers;
  //   if (m_crawlWeibo) {
  //     item.set_weibo(get_weibo(item));
  //     if (m_weiboCallback) {
  //       m_weiboCallback(item.uid, item.weibo);
  //     }
  //   }
  //   m_writer.write_one(item);
  spdlog::info("write uid: {} to mongodb!", user.uid);
  // }
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
  while (m_running) {
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
      std::string video_url;
      if (item.count("page_info") && item["page_info"].count("media_info") && 
          item["page_info"]["media_info"].count("stream_url")) {
        video_url = item["page_info"]["media_info"]["stream_url"].get<std::string>();
        if (video_url.find("http") != 0) {
          video_url = "";
        }
      }
      Weibo weibo(text, tm, id, urls, video_url);
      spdlog::info(weibo.dump());
      weibos.push_back(weibo);
    }
    page_cnt += 1;
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
  return weibos;
}

