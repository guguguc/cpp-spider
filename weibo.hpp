#ifndef WEIBO
#define WEIBO

#include <string>
#include <vector>
#include <cstdint>

class Weibo {
public:
  Weibo(std::string text, std::string timestamp, uint64_t id, std::vector<std::string> pics)
    :text(text), timestamp(timestamp), id(id), pics(pics) {}
  std::string dump();
  std::string text;
  std::string timestamp;
  uint64_t id;
  std::vector<std::string> pics;
};

class User {
public:
  User() {};
  User(uint64_t user_id, const std::string &user_name, const std::vector<User> &followers)
  : uid(user_id), username(user_name), followers(followers) {}
  void set_weibo(const std::vector<Weibo> weibos) { weibo = weibos; }
  uint64_t uid;
  std::string username;
  std::vector<User> followers;
  std::vector<Weibo> weibo;
};

#endif  // WEIBO
