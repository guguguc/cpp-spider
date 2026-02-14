#ifndef SPIDER
#define SPIDER


#include <vector>
#include <functional>
#include <httplib.h>
#include "weibo.hpp"
#include "writer.hpp"


class Spider {
public:
  using UserCallback = std::function<void(uint64_t uid, const std::string& name, 
                                           const std::vector<uint64_t>& followers, 
                                           const std::vector<uint64_t>& fans)>;
  using WeiboCallback = std::function<void(uint64_t uid, const std::vector<Weibo>& weibos)>;

  explicit Spider(uint64_t user_id);
  void setUserCallback(UserCallback callback);
  void setWeiboCallback(WeiboCallback callback);
  void setCrawlWeibo(bool crawl);
  void setCrawlFans(bool crawl);
  void setCrawlFollowers(bool crawl);
  void stop();
  bool isRunning() const { return m_running; }
  User get_user(uint64_t uid, bool get_follower=false);
  std::vector<User> get_self_follower();
  std::vector<User> get_other_follower(uint64_t uid);
  std::vector<Weibo> get_weibo(const User &user);
  void run();
private:
  std::vector<User> batch_get_user(const std::vector<uint64_t> &ids);
  void notifyUserFetched(uint64_t uid, const std::string& name, 
                        const std::vector<uint64_t>& followers, 
                        const std::vector<uint64_t>& fans);
private:
  User m_self;
  std::unique_ptr<httplib::Client> m_client; 
  uint64_t m_visit_cnt;
  MongoWriter m_writer;
  UserCallback m_userCallback;
  WeiboCallback m_weiboCallback;
  bool m_crawlWeibo;
  bool m_crawlFans;
  bool m_crawlFollowers;
  bool m_running;
};

#endif  // SPIDER
