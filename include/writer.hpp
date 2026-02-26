#ifndef MONGOWRITER
#define MONGOWRITER

#include <mongocxx/client-fwd.hpp>
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/collection-fwd.hpp>
#include <mongocxx/database-fwd.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <set>
#include "weibo.hpp"

class MongoWriter {
public:
  MongoWriter(const std::string &uri,
              const std::string &db_name = "weibo",
              const std::string &collection_name = "user");
  void write_one(const User &user);
  void write_many(const std::vector<User> &users);

  // Incremental crawl support
  bool user_exists(uint64_t uid);
  uint64_t get_latest_weibo_id(uint64_t uid);
  std::set<uint64_t> get_stored_weibo_ids(uint64_t uid);

private:
  mongocxx::client m_client;
  mongocxx::database m_db;
  mongocxx::collection m_collection;
};

#endif  // MONGOWRITER
