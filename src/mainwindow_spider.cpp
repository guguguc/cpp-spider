#include "mainwindow.hpp"
#include "spider.hpp"
#include "weibo.hpp"
#include "qt_log_sink.hpp"
#include <QThread>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {
spdlog::level::level_enum parse_log_level(const std::string &level) {
  if (level == "trace") return spdlog::level::trace;
  if (level == "debug") return spdlog::level::debug;
  if (level == "info") return spdlog::level::info;
  if (level == "warn") return spdlog::level::warn;
  if (level == "error") return spdlog::level::err;
  if (level == "critical") return spdlog::level::critical;
  if (level == "off") return spdlog::level::off;
  return spdlog::level::info;
}
}

void MainWindow::onStartClicked() {
  m_running = true;
  m_startBtn->setEnabled(false);
  m_stopBtn->setEnabled(true);
  m_logPanel->appendLog(LogLevel::App, "Starting spider...");
  runSpider();
}

void MainWindow::onStopClicked() {
  m_running = false;
  if (m_spider) { m_spider->stop(); }
  m_startBtn->setEnabled(true);
  m_stopBtn->setEnabled(false);
  m_logPanel->appendLog(LogLevel::App, "Spider stopped.");
}

void MainWindow::appendLog(const QString& message) {
  m_logPanel->appendLog(LogLevel::App, message);
}

void MainWindow::appendSpiderLog(int level, const QString& message) {
  LogLevel logLevel;
  switch (level) {
    case 0: logLevel = LogLevel::Trace; break;
    case 1: logLevel = LogLevel::Debug; break;
    case 2: logLevel = LogLevel::Info; break;
    case 3: logLevel = LogLevel::Warn; break;
    case 4: logLevel = LogLevel::Error; break;
    case 5: logLevel = LogLevel::Critical; break;
    default: logLevel = LogLevel::Info; break;
  }
  m_logPanel->appendLog(logLevel, message, "spider");
}

void MainWindow::onUserFetched(uint64_t uid, const QString& name, const QList<uint64_t>& followers, const QList<uint64_t>& fans) {
  addUserNode(uid, name, followers, fans);
}

void MainWindow::onWeiboFetched(uint64_t uid, const QString& weiboText) {
  WeiboData data;
  data.text = weiboText;
  std::lock_guard<std::mutex> lock(m_weiboMutex);
  m_weibos[uid].push_back(data);
}

void MainWindow::onMetricsUpdated(qulonglong usersProcessed,
                                  qulonglong usersFailed,
                                  qulonglong requestsTotal,
                                  qulonglong requestsFailed,
                                  qulonglong retriesTotal,
                                  qulonglong http429Count,
                                  qulonglong queuePending,
                                  qulonglong visitedTotal,
                                  qulonglong currentUid) {
  if (m_monitorUsersLabel) {
    m_monitorUsersLabel->setText(
        QString("Users: processed=%1 failed=%2")
            .arg(usersProcessed)
            .arg(usersFailed));
  }
  if (m_monitorRequestsLabel) {
    m_monitorRequestsLabel->setText(
        QString("Requests: total=%1 failed=%2")
            .arg(requestsTotal)
            .arg(requestsFailed));
  }
  if (m_monitorRetriesLabel) {
    m_monitorRetriesLabel->setText(QString("Retries: %1").arg(retriesTotal));
  }
  if (m_monitor429Label) {
    m_monitor429Label->setText(QString("HTTP 429: %1").arg(http429Count));
  }
  if (m_monitorQueueLabel) {
    m_monitorQueueLabel->setText(
        QString("Queue: pending=%1 visited=%2")
            .arg(queuePending)
            .arg(visitedTotal));
  }
  if (m_monitorCurrentUidLabel) {
    m_monitorCurrentUidLabel->setText(QString("Current UID: %1").arg(currentUid));
  }
}

void MainWindow::runSpider() {
  syncSettingsUiToAppConfig();
  m_appConfig.save();

  m_crawlWeibo = m_crawlWeiboCheck->isChecked();
  m_targetUid = m_uidInput->text().toULongLong();
  bool crawlFans = m_crawlFansCheck->isChecked();
  bool crawlFollowers = m_crawlFollowersCheck->isChecked();
  spdlog::info(fmt::format(
      "run spider with uid={} depth={} retries={} base={}ms max={}ms backoff={} interval={}ms jitter={}ms cooldown429={}ms log_level={}",
      m_targetUid,
      m_appConfig.crawl_max_depth,
      m_appConfig.retry_max_attempts,
      m_appConfig.retry_base_delay_ms,
      m_appConfig.retry_max_delay_ms,
      m_appConfig.retry_backoff_factor,
      m_appConfig.request_min_interval_ms,
      m_appConfig.request_jitter_ms,
      m_appConfig.cooldown_429_ms,
      m_appConfig.log_level));
  spdlog::info(fmt::format(
      "run spider paths: cookie_path={}, headers_path={}, config_path={}",
      m_appConfig.cookie_path,
      m_appConfig.headers_path,
      m_appConfig.config_path));
  saveConfig();

  QThread* thread = QThread::create([this, crawlFans, crawlFollowers]() {
    try {
      m_spider = std::make_unique<Spider>(m_targetUid, m_appConfig);
      m_spider->setCrawlWeibo(m_crawlWeibo);
      m_spider->setCrawlFans(crawlFans);
      m_spider->setCrawlFollowers(crawlFollowers);
      m_spider->setMaxDepth(m_appConfig.crawl_max_depth);
      m_spider->setUserCallback([this](uint64_t uid, const std::string& name,
                                       const std::vector<uint64_t>& followers,
                                       const std::vector<uint64_t>& fans) {
        QList<uint64_t> followersList;
        for (auto f : followers) followersList.append(f);
        QList<uint64_t> fansList;
        for (auto f : fans) fansList.append(f);
        QMetaObject::invokeMethod(this, "onUserFetched", Qt::BlockingQueuedConnection,
                                  Q_ARG(uint64_t, uid),
                                  Q_ARG(QString, QString::fromStdString(name)),
                                  Q_ARG(QList<uint64_t>, followersList),
                                  Q_ARG(QList<uint64_t>, fansList));
      });

      m_spider->setMetricsCallback([this](uint64_t usersProcessed,
                                          uint64_t usersFailed,
                                          uint64_t requestsTotal,
                                          uint64_t requestsFailed,
                                          uint64_t retriesTotal,
                                          uint64_t http429Count,
                                          uint64_t queuePending,
                                          uint64_t visitedTotal,
                                          uint64_t currentUid) {
        QMetaObject::invokeMethod(this, "onMetricsUpdated", Qt::QueuedConnection,
                                  Q_ARG(qulonglong, static_cast<qulonglong>(usersProcessed)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(usersFailed)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(requestsTotal)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(requestsFailed)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(retriesTotal)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(http429Count)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(queuePending)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(visitedTotal)),
                                  Q_ARG(qulonglong, static_cast<qulonglong>(currentUid)));
      });

      m_spider->setWeiboCallback([this](uint64_t uid, const std::vector<Weibo>& weibos) {
        std::lock_guard<std::mutex> lock(m_weiboMutex);
        for (const auto& w : weibos) {
          WeiboData data;
          data.timestamp = QString::fromStdString(w.timestamp);
          data.text = QString::fromStdString(w.text).toHtmlEscaped();
          data.pics = w.pics;
          data.video_url = w.video_url;
          m_weibos[uid].push_back(data);
        }
        
        int totalWeibo = 0;
        int totalVideo = 0;
        for (const auto& uidWeibos : m_weibos) {
          for (const auto& weibo : uidWeibos) {
            totalWeibo++;
            if (!weibo.video_url.empty() && weibo.video_url.find("http") == 0)
              totalVideo++;
          }
        }
        QMetaObject::invokeMethod(this, "updateWeiboStats", Qt::QueuedConnection,
                                  Q_ARG(int, totalWeibo), Q_ARG(int, totalVideo));
      });

      if (m_running) {
        QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                  Q_ARG(QString, QString::fromStdString("Fetching user data...")));
        spdlog::debug(fmt::format(
            "creating spider with cookie_path={} headers_path={} config_path={}",
            m_appConfig.cookie_path,
            m_appConfig.headers_path,
            m_appConfig.config_path));
        m_spider->run();
        if (m_running) {
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                    Q_ARG(QString, QString::fromStdString("Spider completed!")));
        }
      }
    } catch (const std::exception& e) {
      spdlog::error(fmt::format("runSpider exception: {}", e.what()));
      QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                Q_ARG(QString, QString::fromStdString(fmt::format(
                                    "Error: {} (cookie: {}, headers: {})",
                                    e.what(),
                                    m_appConfig.cookie_path,
                                    m_appConfig.headers_path))));
    }
    QMetaObject::invokeMethod(this, "onStopClicked", Qt::QueuedConnection);
  });

  connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  thread->start();
}

void MainWindow::syncSettingsUiToAppConfig() {
  if (!m_retryAttemptsSpin || !m_retryBaseDelaySpin || !m_retryMaxDelaySpin ||
      !m_retryBackoffSpin || !m_requestMinIntervalSpin ||
      !m_requestJitterSpin || !m_cooldown429Spin) {
    return;
  }

  if (m_depthSpin) {
    m_appConfig.crawl_max_depth = m_depthSpin->value();
  }

  m_appConfig.retry_max_attempts = m_retryAttemptsSpin->value();
  m_appConfig.retry_base_delay_ms = m_retryBaseDelaySpin->value();
  m_appConfig.retry_max_delay_ms = m_retryMaxDelaySpin->value();
  m_appConfig.retry_backoff_factor = m_retryBackoffSpin->value();
  m_appConfig.request_min_interval_ms = m_requestMinIntervalSpin->value();
  m_appConfig.request_jitter_ms = m_requestJitterSpin->value();
  m_appConfig.cooldown_429_ms = m_cooldown429Spin->value();
  if (m_logLevelCombo) {
    m_appConfig.log_level = m_logLevelCombo->currentText().toStdString();
  }
}

void MainWindow::loadCookieEditor() {
  if (!m_cookieEditor || !m_cookiePathLabel) {
    return;
  }

  const QString cookie_path = QString::fromStdString(m_appConfig.cookie_path);
  m_cookiePathLabel->setText(QString("Cookie Path: %1").arg(cookie_path));

  QFile file(cookie_path);
  if (!file.exists()) {
    m_cookieEditor->clear();
    appendLog(QString("Cookie file not found: %1").arg(cookie_path));
    spdlog::warn(fmt::format("cookie file not found: {}", m_appConfig.cookie_path));
    return;
  }

  if (!file.open(QIODevice::ReadOnly)) {
    appendLog(QString("Failed to open cookie file: %1").arg(cookie_path));
    spdlog::error(fmt::format("failed to open cookie file: {}", m_appConfig.cookie_path));
    return;
  }

  QByteArray data = file.readAll();
  file.close();
  m_cookieEditor->setPlainText(QString::fromUtf8(data));
  appendLog(QString("Cookie loaded (%1 bytes)").arg(data.size()));
  spdlog::debug(fmt::format("cookie loaded from {} ({} bytes)", m_appConfig.cookie_path, data.size()));
}

void MainWindow::saveCookieEditor() {
  if (!m_cookieEditor) {
    return;
  }

  const QString cookie_path = QString::fromStdString(m_appConfig.cookie_path);
  const QByteArray raw = m_cookieEditor->toPlainText().toUtf8();
  QJsonParseError error;
  QJsonDocument doc = QJsonDocument::fromJson(raw, &error);
  if (error.error != QJsonParseError::NoError || !doc.isObject()) {
    appendLog(QString("Cookie JSON invalid: %1").arg(error.errorString()));
    spdlog::error(fmt::format("cookie json invalid: {}", error.errorString().toStdString()));
    return;
  }

  QFileInfo file_info(cookie_path);
  QDir dir = file_info.dir();
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  QFile file(cookie_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    appendLog(QString("Failed to write cookie file: %1").arg(cookie_path));
    spdlog::error(fmt::format("failed to write cookie file: {}", m_appConfig.cookie_path));
    return;
  }

  const QByteArray pretty = doc.toJson(QJsonDocument::Indented);
  file.write(pretty);
  file.close();
  m_cookieEditor->setPlainText(QString::fromUtf8(pretty));
  appendLog(QString("Cookie saved: %1").arg(cookie_path));
  spdlog::info(fmt::format("cookie saved to {}", m_appConfig.cookie_path));
}

void MainWindow::updateWeiboStats(int totalWeibo, int totalVideo) {
  m_totalWeiboLabel->setText(fmt::format("Total Weibo: {}", totalWeibo).c_str());
  m_totalVideoLabel->setText(fmt::format("Total Video: {}", totalVideo).c_str());
}

void MainWindow::loadConfig() {
  QFile file(QString::fromStdString(m_appConfig.config_path));
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray data = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull() && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("crawl_weibo"))
        m_crawlWeibo = obj["crawl_weibo"].toBool(true);
      if (obj.contains("target_uid"))
        m_targetUid = obj["target_uid"].toVariant().toULongLong();
      if (obj.contains("crawl_depth"))
        m_appConfig.crawl_max_depth = obj["crawl_depth"].toInt(m_appConfig.crawl_max_depth);
      if (obj.contains("theme_index")) {
        int theme_index = obj["theme_index"].toInt(0);
        if (theme_index >= 0 && theme_index < m_themes.size())
          m_currentTheme = theme_index;
      }
    }
  }
}

void MainWindow::saveConfig() {
  QJsonObject obj;
  obj["crawl_weibo"] = m_crawlWeiboCheck->isChecked();
  obj["target_uid"] = QString::number(m_targetUid);
  obj["crawl_depth"] = m_depthSpin ? m_depthSpin->value() : m_appConfig.crawl_max_depth;
  obj["theme_index"] = m_currentTheme;
  QJsonDocument doc(obj);
  QFile file(QString::fromStdString(m_appConfig.config_path));
  if (file.open(QIODevice::WriteOnly)) {
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
  }
}

std::string MainWindow::loadCookies() {
  std::ifstream cookie_ifs(m_appConfig.cookie_path);
  if (!cookie_ifs.is_open()) return "";
  using json = nlohmann::json;
  json json_cookie = json::parse(cookie_ifs);
  std::string cookie;
  for (json::iterator it = json_cookie.begin(); it != json_cookie.end(); ++it) {
    if (!cookie.empty()) cookie += "; ";
    cookie += it.key() + "=" + it.value().get<std::string>();
  }
  cookie_ifs.close();
  return cookie;
}

void MainWindow::setupLogSink() {
  auto sink = std::make_shared<QtLogSinkMt>(
      [this](int level, const std::string& msg) {
        QMetaObject::invokeMethod(this, "appendSpiderLog",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, level),
                                  Q_ARG(QString, QString::fromStdString(msg)));
      });
  sink->set_pattern("%v");
  auto logger = std::make_shared<spdlog::logger>("spider", sink);
  const auto level = parse_log_level(m_appConfig.log_level);
  logger->set_level(level);
  logger->flush_on(spdlog::level::warn);
  spdlog::set_default_logger(logger);
  spdlog::info(fmt::format("logger initialized with level={}", m_appConfig.log_level));
}
