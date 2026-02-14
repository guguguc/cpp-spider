#include "weibo.hpp"
#include <fmt/format.h>

std::string Weibo::dump()
{
  return fmt::format("id: {}, text: {}, timestamp: {}", id, text, timestamp);
}
