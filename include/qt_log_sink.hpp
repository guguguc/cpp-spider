#ifndef QT_LOG_SINK_HPP
#define QT_LOG_SINK_HPP

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <mutex>
#include <functional>
#include <string>

// Callback type: (spdlog_level, message)
using QtLogCallback = std::function<void(int, const std::string&)>;

template <typename Mutex>
class QtLogSink : public spdlog::sinks::base_sink<Mutex> {
public:
  explicit QtLogSink(QtLogCallback callback)
      : m_callback(std::move(callback)) {}

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    spdlog::memory_buf_t formatted;
    spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
    std::string text = fmt::to_string(formatted);
    // Remove trailing newline
    if (!text.empty() && text.back() == '\n') {
      text.pop_back();
    }
    if (m_callback) {
      m_callback(static_cast<int>(msg.level), text);
    }
  }

  void flush_() override {}

private:
  QtLogCallback m_callback;
};

using QtLogSinkMt = QtLogSink<std::mutex>;
using QtLogSinkSt = QtLogSink<spdlog::details::null_mutex>;

#endif  // QT_LOG_SINK_HPP
