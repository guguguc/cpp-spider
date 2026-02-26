#include "mainwindow.hpp"
#include "spider.hpp"
#include "weibo.hpp"
#include "qt_log_sink.hpp"
#include <QThread>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

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
  m_weibos[uid].push_back(data);
}

void MainWindow::runSpider() {
  m_crawlWeibo = m_crawlWeiboCheck->isChecked();
  m_targetUid = m_uidInput->text().toULongLong();
  bool crawlFans = m_crawlFansCheck->isChecked();
  bool crawlFollowers = m_crawlFollowersCheck->isChecked();
  saveConfig();

  QThread* thread = QThread::create([this, crawlFans, crawlFollowers]() {
    try {
      m_spider = std::make_unique<Spider>(m_targetUid, m_appConfig);
      m_spider->setCrawlWeibo(m_crawlWeibo);
      m_spider->setCrawlFans(crawlFans);
      m_spider->setCrawlFollowers(crawlFollowers);
      m_spider->setUserCallback([this](uint64_t uid, const std::string& name,
                                       const std::vector<uint64_t>& followers,
                                       const std::vector<uint64_t>& fans) {
        QList<uint64_t> followersList;
        for (auto f : followers) followersList.append(f);
        QList<uint64_t> fansList;
        for (auto f : fans) fansList.append(f);
        QMetaObject::invokeMethod(this, "onUserFetched", Qt::QueuedConnection,
                                  Q_ARG(uint64_t, uid),
                                  Q_ARG(QString, QString::fromStdString(name)),
                                  Q_ARG(QList<uint64_t>, followersList),
                                  Q_ARG(QList<uint64_t>, fansList));
      });

      m_spider->setWeiboCallback([this](uint64_t uid, const std::vector<Weibo>& weibos) {
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
        m_spider->run();
        if (m_running) {
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                    Q_ARG(QString, QString::fromStdString("Spider completed!")));
        }
      }
    } catch (const std::exception& e) {
      QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                                Q_ARG(QString, QString::fromStdString(fmt::format("Error: {}", e.what()))));
    }
    QMetaObject::invokeMethod(this, "onStopClicked", Qt::QueuedConnection);
  });

  connect(thread, &QThread::finished, thread, &QThread::deleteLater);
  thread->start();
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
  logger->set_level(spdlog::level::trace);
  spdlog::set_default_logger(logger);
}
