#ifndef SPIDER
#define SPIDER


#include <vector>
#include <httplib.h>
#include "weibo.hpp"
#include "writer.hpp"


class Spider {
public:
  Spider(uint64_t user_id);
  User get_user(uint64_t uid, bool get_follower=false);
  std::vector<User> get_self_follower();
  std::vector<User> get_other_follower(uint64_t uid);
  std::vector<Weibo> get_weibo(const User &user);
  void run();
private:
  std::vector<User> batch_get_user(const std::vector<uint64_t> &ids);
private:
  User m_self;
  std::unique_ptr<httplib::Client> m_client; 
  uint64_t m_visit_cnt;
  MongoWriter m_writer;
};

#endif  // SPIDER
