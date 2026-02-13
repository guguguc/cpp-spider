#include "spider.hpp"

const std::string mongo_url = "mongodb://0.0.0.0:27017";

int main()
{
  mongocxx::instance inst{};
  Spider spider(6126303533);
  spider.run();
    
  return 0;
}
