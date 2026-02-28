#ifndef SPIDER
#define SPIDER


#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <httplib.h>
#include "app_config.hpp"
#include "weibo.hpp"


class MongoWriter;

std::vector<Weibo> load_weibos_from_db(const AppConfig &config, uint64_t uid);

class Spider {
public:
  using UserCallback = std::function<void(uint64_t uid, const std::string& name, 
                                           const std::vector<uint64_t>& followers, 
                                           const std::vector<uint64_t>& fans)>;
  using WeiboCallback = std::function<void(uint64_t uid, const std::vector<Weibo>& weibos)>;
  using MetricsCallback = std::function<void(uint64_t users_processed,
                                             uint64_t users_failed,
                                             uint64_t requests_total,
                                             uint64_t requests_failed,
                                             uint64_t retries_total,
                                             uint64_t http_429_count,
                                             uint64_t queue_pending,
                                             uint64_t visited_total,
                                             uint64_t current_uid)>;

  explicit Spider(uint64_t user_id, const AppConfig &config);
  ~Spider();
  void setUserCallback(UserCallback callback);
  void setWeiboCallback(WeiboCallback callback);
  void setCrawlWeibo(bool crawl);
  void setCrawlFans(bool crawl);
  void setCrawlFollowers(bool crawl);
  void setMaxDepth(int max_depth);
  void setMetricsCallback(MetricsCallback callback);

  void stop();
  bool isRunning() const { return m_running; }
  User get_user(uint64_t uid,
                bool get_follower=false,
                std::vector<uint64_t> *follower_ids_out=nullptr,
                std::vector<uint64_t> *fan_ids_out=nullptr);
  std::vector<User> get_self_follower(uint64_t uid);
  std::vector<User> get_other_follower(uint64_t uid);
  std::vector<Weibo> get_weibo(const User &user);
  void run();
private:
  std::vector<User> batch_get_user(const std::vector<uint64_t> &ids);
  void notifyUserFetched(uint64_t uid, const std::string& name, 
                        const std::vector<uint64_t>& followers, 
                        const std::vector<uint64_t>& fans);
  httplib::Result get_with_retry(const std::string &url,
                                 const std::string &request_name);
  bool is_retryable_result(const httplib::Result &result) const;
  int get_retry_delay_ms(int attempt) const;
  void wait_for_request_slot() const;
  int get_jitter_delay_ms() const;
  void emit_metrics(bool force = false);
  bool load_crawl_state(std::vector<std::pair<uint64_t, int>> *queue,
                        size_t *cursor,
                        std::set<uint64_t> *visited);
  void save_crawl_state(const std::vector<std::pair<uint64_t, int>> &queue,
                        size_t cursor,
                        const std::set<uint64_t> &visited,
                        uint64_t current_uid);
  void clear_crawl_state();
private:
  User m_self;
  std::unique_ptr<httplib::Client> m_client; 
  uint64_t m_visit_cnt;
  std::unique_ptr<MongoWriter> m_writer;
  UserCallback m_userCallback;
  WeiboCallback m_weiboCallback;
  MetricsCallback m_metricsCallback;
  bool m_crawlWeibo;
  bool m_crawlFans;
  bool m_crawlFollowers;
  int m_max_depth;
  bool m_running;
  std::string m_state_path;
  uint64_t m_users_processed;
  uint64_t m_users_failed;
  uint64_t m_requests_total;
  uint64_t m_requests_failed;
  uint64_t m_retries_total;
  uint64_t m_http_429_count;
  uint64_t m_current_uid;
  uint64_t m_queue_pending;
  uint64_t m_visited_total;
  int m_retry_max_attempts;
  int m_retry_base_delay_ms;
  int m_retry_max_delay_ms;
  double m_retry_backoff_factor;
  int m_request_min_interval_ms;
  int m_request_jitter_ms;
  int m_cooldown_429_ms;
  mutable std::chrono::steady_clock::time_point m_next_request_time;
  mutable std::mt19937 m_rng;
  mutable std::mutex m_rate_limit_mutex;

};

#endif  // SPIDER
