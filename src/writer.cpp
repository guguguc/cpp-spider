#include "writer.hpp"
#include <bsoncxx/builder/basic/array-fwd.hpp>
#include <bsoncxx/builder/basic/document-fwd.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client-fwd.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/update.hpp>
#include <mongocxx/options/index.hpp>
#include <fmt/core.h>
#include <string>
MongoWriter::MongoWriter(const std::string &uri,
                         const std::string &db_name,
                         const std::string &collection_name)
{
  mongocxx::options::client client_option;
  auto api = mongocxx::options::server_api{mongocxx::options::server_api::version::k_version_1};
  client_option.server_api_opts(api);
  m_client = mongocxx::client{mongocxx::uri(uri), client_option};
  auto dbs = m_client.list_database_names();
  if (std::find(dbs.begin(), dbs.end(), db_name) == dbs.end())
  {
    m_client[db_name];
  }
  m_db = m_client[db_name];
  auto collections = m_db.list_collection_names();
  if (std::find(collections.begin(), collections.end(), collection_name) == collections.end()) {
    m_db.create_collection(collection_name);
  }
  m_collection = m_db[collection_name];

  // Ensure uid unique index to prevent duplicate documents
  using bsoncxx::builder::basic::kvp;
  bsoncxx::builder::basic::document index_key;
  index_key.append(kvp("uid", 1));
  mongocxx::options::index index_opts;
  index_opts.unique(true);
  try {
    m_collection.create_index(index_key.view(), index_opts);
  } catch (const std::exception &e) {
    spdlog::warn(fmt::format("uid index creation: {}", e.what()));
  }
}

void MongoWriter::write_one(const User &user)
{
  using bsoncxx::builder::basic::kvp;

  // Get existing weibo IDs to deduplicate
  auto existing_ids = get_stored_weibo_ids(user.uid);

  // Build only new weibos
  bsoncxx::builder::basic::array new_weibos;
  int new_count = 0;
  for (const auto &weibo : user.weibo) {
    if (existing_ids.count(weibo.id)) continue;
    bsoncxx::builder::basic::document wb;
    bsoncxx::builder::basic::array pics;
    for (const auto &pic : weibo.pics) {
      pics.append(pic);
    }
    wb.append(kvp("id", std::to_string(weibo.id)));
    wb.append(kvp("timestamp", weibo.timestamp));
    wb.append(kvp("text", weibo.text));
    wb.append(kvp("pics", pics.extract()));
    wb.append(kvp("video_url", weibo.video_url));
    new_weibos.append(wb.extract());
    new_count++;
  }

  // Build followers array
  bsoncxx::builder::basic::array followers;
  for (const auto &follower : user.followers) {
    followers.append(std::to_string(follower.uid));
  }

  bsoncxx::builder::basic::document filter;
  filter.append(kvp("uid", std::to_string(user.uid)));

  if (!user_exists(user.uid)) {
    // First time: insert new document
    bsoncxx::builder::basic::document doc;
    doc.append(kvp("uid", std::to_string(user.uid)));
    doc.append(kvp("username", user.username));
    doc.append(kvp("followers", followers.extract()));
    doc.append(kvp("weibos", new_weibos.extract()));
    m_collection.insert_one(doc.view());
    spdlog::info(fmt::format(
        "inserted new user uid:{} with {} weibos",
        user.uid, new_count));
  } else {
    // User exists: update info + append new weibos only
    bsoncxx::builder::basic::document set_doc;
    set_doc.append(kvp("username", user.username));
    if (!user.followers.empty()) {
      set_doc.append(kvp("followers", followers.extract()));
    }

    bsoncxx::builder::basic::document update;
    update.append(kvp("$set", set_doc.extract()));

    if (new_count > 0) {
      bsoncxx::builder::basic::document each_doc;
      each_doc.append(kvp("$each", new_weibos.extract()));
      bsoncxx::builder::basic::document push_doc;
      push_doc.append(kvp("weibos", each_doc.extract()));
      update.append(kvp("$push", push_doc.extract()));
    }

    m_collection.update_one(filter.view(), update.view());
    spdlog::info(fmt::format(
        "updated user uid:{}, {} new weibos appended",
        user.uid, new_count));
  }
}

void MongoWriter::write_many(const std::vector<User> &users)
{
  for (const auto &user : users) {
    write_one(user);
  }
}

bool MongoWriter::user_exists(uint64_t uid) {
  using bsoncxx::builder::basic::kvp;
  bsoncxx::builder::basic::document filter;
  filter.append(kvp("uid", std::to_string(uid)));
  auto result = m_collection.find_one(filter.view());
  return result.has_value();
}

uint64_t MongoWriter::get_latest_weibo_id(uint64_t uid) {
  using bsoncxx::builder::basic::kvp;
  bsoncxx::builder::basic::document filter;
  filter.append(kvp("uid", std::to_string(uid)));

  auto result = m_collection.find_one(filter.view());
  if (!result) return 0;

  auto doc = result->view();
  if (!doc["weibos"] || doc["weibos"].type() != bsoncxx::type::k_array) {
    return 0;
  }

  uint64_t max_id = 0;
  for (const auto &elem : doc["weibos"].get_array().value) {
    if (elem.type() != bsoncxx::type::k_document) continue;
    auto wb = elem.get_document().value;
    if (wb["id"]) {
      uint64_t id = std::stoull(std::string(wb["id"].get_string().value));
      if (id > max_id) max_id = id;
    }
  }
  return max_id;
}

std::set<uint64_t> MongoWriter::get_stored_weibo_ids(uint64_t uid) {
  using bsoncxx::builder::basic::kvp;
  std::set<uint64_t> ids;
  bsoncxx::builder::basic::document filter;
  filter.append(kvp("uid", std::to_string(uid)));

  // Use find (not find_one) to scan ALL documents with this uid
  // This handles legacy duplicate documents from old insert_one behavior
  auto cursor = m_collection.find(filter.view());
  for (const auto &doc : cursor) {
    if (!doc["weibos"] || doc["weibos"].type() != bsoncxx::type::k_array) {
      continue;
    }
    for (const auto &elem : doc["weibos"].get_array().value) {
      if (elem.type() != bsoncxx::type::k_document) continue;
      auto wb = elem.get_document().value;
      if (wb["id"]) {
        uint64_t id = std::stoull(
            std::string(wb["id"].get_string().value));
        ids.insert(id);
      }
    }
  }
  return ids;
}
