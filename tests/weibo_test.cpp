#include "weibo.hpp"

#include <gtest/gtest.h>

TEST(WeiboTest, DumpIncludesCoreFields) {
  Weibo weibo("hello", "2026-01-02 03:04:05", 42, {"pic1", "pic2"}, "video");
  const std::string dumped = weibo.dump();

  EXPECT_NE(dumped.find("id: 42"), std::string::npos);
  EXPECT_NE(dumped.find("text: hello"), std::string::npos);
  EXPECT_NE(dumped.find("timestamp: 2026-01-02 03:04:05"), std::string::npos);
}

TEST(UserTest, SetWeiboReplacesCurrentCollection) {
  User user(7, "alice", {});
  std::vector<Weibo> first = {Weibo("a", "t1", 1, {}, "")};
  std::vector<Weibo> second = {
      Weibo("b", "t2", 2, {}, ""),
      Weibo("c", "t3", 3, {}, "")};

  user.set_weibo(first);
  ASSERT_EQ(user.weibo.size(), 1U);
  EXPECT_EQ(user.weibo[0].id, 1U);

  user.set_weibo(second);
  ASSERT_EQ(user.weibo.size(), 2U);
  EXPECT_EQ(user.weibo[0].id, 2U);
  EXPECT_EQ(user.weibo[1].id, 3U);
}
