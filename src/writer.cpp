#include "writer.hpp"
#include <bsoncxx/builder/basic/array-fwd.hpp>
#include <bsoncxx/builder/basic/document-fwd.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client-fwd.hpp>
#include <string>
MongoWriter::MongoWriter(const std::string &uri)
{
  mongocxx::options::client client_option;
  auto api = mongocxx::options::server_api{mongocxx::options::server_api::version::k_version_1};
  client_option.server_api_opts(api);
  m_client = mongocxx::client{mongocxx::uri(uri), client_option};
  auto dbs = m_client.list_database_names();
  if (std::find(dbs.begin(), dbs.end(), "weibo") == dbs.end())
  {
    m_client["weibo"];
  }
  m_db = m_client["weibo"];
  auto collections = m_db.list_collection_names();
  if (std::find(collections.begin(), collections.end(), "user") == collections.end()) {
    m_db.create_collection("user");
  }
  m_collection = m_db["user"];
}

void MongoWriter::write_one(const User &user)
{
  bsoncxx::builder::basic::array followers;
  bsoncxx::builder::basic::array weibos;
  bsoncxx::builder::basic::document doc;
  doc.append(bsoncxx::builder::basic::kvp("uid", std::to_string(user.uid)));
  doc.append(bsoncxx::builder::basic::kvp("username", user.username));
  for (const auto &follower : user.followers) {
    followers.append(std::to_string(follower.uid));
  }
  for (const auto &weibo: user.weibo) {
    bsoncxx::builder::basic::document wb;
    bsoncxx::builder::basic::array pics;
    for (const auto &pic: weibo.pics) {
      pics.append(pic);
    }
    wb.append(bsoncxx::builder::basic::kvp("id", std::to_string(weibo.id)));
    wb.append(bsoncxx::builder::basic::kvp("timestamp", weibo.timestamp));
    wb.append(bsoncxx::builder::basic::kvp("text", weibo.text));
    wb.append(bsoncxx::builder::basic::kvp("pics", pics.extract()));
    weibos.append(wb.extract());
  }
  doc.append(bsoncxx::builder::basic::kvp("weibos", weibos.extract()));
  m_client["weibo"]["user"].insert_one(doc.view());
}

void MongoWriter::write_many(const std::vector<User> &users)
{
  for (const auto &user : users) {
    write_one(user);
  }
}
