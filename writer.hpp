
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
#include "weibo.hpp"

class MongoWriter {
public:
  MongoWriter(const std::string &uri);
  void write_one(const User &user);
  void write_many(const std::vector<User> &users);

private:
  mongocxx::client m_client;
  mongocxx::database m_db;
  mongocxx::collection m_collection;
};

#endif  // MONGOWRITER
