#include "mainwindow.hpp"
#include "spider.hpp"
#include <algorithm>
#include <QBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QPointer>
#include <QScrollArea>
#include <QHeaderView>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <fstream>
#include <thread>
#include <httplib.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

void MainWindow::ensureWeibosLoaded(uint64_t uid) {
  {
    std::lock_guard<std::mutex> lock(m_weiboMutex);
    if (m_weibos.contains(uid) && !m_weibos[uid].empty()) {
      return;
    }
  }

  try {
    std::vector<Weibo> db_weibos = load_weibos_from_db(m_appConfig, uid);
    if (db_weibos.empty()) {
      appendLog(QString("No weibo found in DB for uid %1").arg(uid));
      return;
    }

    std::vector<WeiboData> cache;
    cache.reserve(db_weibos.size());
    for (const auto &w : db_weibos) {
      WeiboData data;
      data.timestamp = QString::fromStdString(w.timestamp);
      data.text = QString::fromStdString(w.text);
      data.pics = w.pics;
      data.video_url = w.video_url;
      cache.push_back(std::move(data));
    }
    {
      std::lock_guard<std::mutex> lock(m_weiboMutex);
      m_weibos[uid] = std::move(cache);
    }
    appendLog(QString("Loaded %1 weibos from DB for uid %2")
                  .arg(static_cast<int>(db_weibos.size()))
                  .arg(uid));
  } catch (const std::exception &e) {
    appendLog(QString("Failed to load weibos from DB for uid %1: %2")
                  .arg(uid)
                  .arg(e.what()));
    spdlog::error(fmt::format("load weibos from db failed, uid={}, error={}", uid, e.what()));
  }
}

void MainWindow::showNodeWeibo(uint64_t uid) {
  const int weibo_page_size = 40;

  if (m_currentWeiboUid != uid) {
    m_currentWeiboUid = uid;
    m_weiboCurrentPage = 0;
  }

  ensureWeibosLoaded(uid);
  m_tabWidget->setCurrentIndex(1);
  
  if (!m_weiboScroll || !m_weiboContainer) return;
  
  QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(m_weiboContainer->layout());
  if (!layout) {
    layout = new QVBoxLayout(m_weiboContainer);
    layout->setAlignment(Qt::AlignTop);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);
  }
  
  while (layout->count() > 0) {
    QLayoutItem* item = layout->takeAt(0);
    if (item->widget()) delete item->widget();
    delete item;
  }
  
  QString title = m_labels.contains(uid) ? m_labels[uid]->toPlainText() : QString::number(uid);
  QWidget* headerWidget = new QWidget(m_weiboContainer);
  headerWidget->setStyleSheet("QWidget { background: linear-gradient(135deg, #45475a 0%, #313244 100%); border-radius: 10px; }");
  QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
  headerLayout->setContentsMargins(20, 20, 20, 20);
  headerLayout->setSpacing(12);
  
  QLabel* titleLabel = new QLabel(QString("<span style='font-size: 20px; font-weight: bold; color: #89b4fa;'>👤 %1</span>").arg(title), headerWidget);
  titleLabel->setTextFormat(Qt::RichText);
  headerLayout->addWidget(titleLabel);
  
  QLabel* uidLabel = new QLabel(QString("<span style='color: #6c7086; font-size: 13px;'>ID: %1</span>").arg(uid), headerWidget);
  uidLabel->setTextFormat(Qt::RichText);
  headerLayout->addWidget(uidLabel);

  std::vector<WeiboData> weibos;
  {
    std::lock_guard<std::mutex> lock(m_weiboMutex);
    if (m_weibos.contains(uid)) {
      weibos = m_weibos[uid];
    }
  }

  if (!weibos.empty()) {
    QHBoxLayout* actionButtonLayout = new QHBoxLayout();
    actionButtonLayout->setContentsMargins(0, 8, 0, 0);
    actionButtonLayout->setSpacing(8);

    QPushButton* viewPicBtn = new QPushButton("🖼️ View All Pictures", headerWidget);
    viewPicBtn->setStyleSheet(
      "QPushButton { background: #89b4fa; color: #1e1e2e; border: 1px solid #89b4fa; padding: 8px 14px; border-radius: 6px; font-weight: bold; font-size: 12px; } "
      "QPushButton:hover { background: #b4befe; border: 1px solid #b4befe; } "
      "QPushButton:pressed { background: #89b4fa; }");
    connect(viewPicBtn, &QPushButton::clicked, [this, uid]() {
      QMetaObject::invokeMethod(this, "showAllPictures", Qt::QueuedConnection, Q_ARG(uint64_t, uid));
    });
    actionButtonLayout->addWidget(viewPicBtn);

    QPushButton* viewVideoBtn = new QPushButton("🎬 View All Videos", headerWidget);
    viewVideoBtn->setStyleSheet(
      "QPushButton { background: #cba6f7; color: #1e1e2e; border: 1px solid #cba6f7; padding: 8px 14px; border-radius: 6px; font-weight: bold; font-size: 12px; } "
      "QPushButton:hover { background: #f5c2e7; border: 1px solid #f5c2e7; } "
      "QPushButton:pressed { background: #cba6f7; }");
    connect(viewVideoBtn, &QPushButton::clicked, [this, uid]() {
      QMetaObject::invokeMethod(this, "showAllVideos", Qt::QueuedConnection, Q_ARG(uint64_t, uid));
    });
    actionButtonLayout->addWidget(viewVideoBtn);
    actionButtonLayout->addStretch();
    headerLayout->addLayout(actionButtonLayout);
  }

  layout->addWidget(headerWidget);

  const size_t total_count = weibos.size();
  const size_t start_index = std::min(
      static_cast<size_t>(m_weiboCurrentPage * weibo_page_size),
      total_count);
  const size_t end_index = std::min(start_index + static_cast<size_t>(weibo_page_size), total_count);
  const size_t render_count = end_index > start_index ? end_index - start_index : 0;

  if (total_count > 0) {
    QLabel* limitLabel = new QLabel(
        QString("Showing %1-%2 of %3 weibos")
            .arg(render_count > 0 ? static_cast<int>(start_index + 1) : 0)
            .arg(static_cast<int>(end_index))
            .arg(static_cast<int>(total_count)),
        m_weiboContainer);
    limitLabel->setStyleSheet(
        "color: #f9e2af; background: #313244; border: 1px solid #45475a; border-radius: 8px; padding: 8px 12px;");
    layout->addWidget(limitLabel);

    QWidget* pagerWidget = new QWidget(m_weiboContainer);
    QHBoxLayout* pagerLayout = new QHBoxLayout(pagerWidget);
    pagerLayout->setContentsMargins(0, 0, 0, 0);
    pagerLayout->setSpacing(8);

    QPushButton* prevBtn = new QPushButton("Prev Page", pagerWidget);
    prevBtn->setStyleSheet(
        "QPushButton { background: #45475a; color: #cdd6f4; border: 1px solid #6c7086; padding: 6px 12px; border-radius: 6px; } "
        "QPushButton:hover { background: #585b70; }");
    prevBtn->setEnabled(m_weiboCurrentPage > 0);
    connect(prevBtn, &QPushButton::clicked, [this, uid]() {
      if (m_weiboCurrentPage > 0) {
        m_weiboCurrentPage -= 1;
        QMetaObject::invokeMethod(this, "showNodeWeibo", Qt::QueuedConnection,
                                  Q_ARG(uint64_t, uid));
      }
    });
    pagerLayout->addWidget(prevBtn);

    QPushButton* nextBtn = new QPushButton(
        QString("Next Page (%1)").arg(weibo_page_size),
        pagerWidget);
    nextBtn->setStyleSheet(
        "QPushButton { background: #89b4fa; color: #1e1e2e; border: 1px solid #89b4fa; padding: 6px 12px; border-radius: 6px; font-weight: bold; } "
        "QPushButton:hover { background: #b4befe; border: 1px solid #b4befe; }");
    nextBtn->setEnabled(end_index < total_count);
    connect(nextBtn, &QPushButton::clicked, [this, uid, end_index, total_count]() {
      if (end_index < total_count) {
        m_weiboCurrentPage += 1;
        QMetaObject::invokeMethod(this, "showNodeWeibo", Qt::QueuedConnection,
                                  Q_ARG(uint64_t, uid));
      }
    });
    pagerLayout->addWidget(nextBtn);

    QPushButton* firstPageBtn = new QPushButton("First Page", pagerWidget);
    firstPageBtn->setStyleSheet(
        "QPushButton { background: #45475a; color: #cdd6f4; border: 1px solid #6c7086; padding: 6px 12px; border-radius: 6px; } "
        "QPushButton:hover { background: #585b70; }");
    firstPageBtn->setEnabled(m_weiboCurrentPage > 0);
    connect(firstPageBtn, &QPushButton::clicked, [this, uid]() {
      m_weiboCurrentPage = 0;
      QMetaObject::invokeMethod(this, "showNodeWeibo", Qt::QueuedConnection,
                                Q_ARG(uint64_t, uid));
    });
    pagerLayout->addWidget(firstPageBtn);

    pagerLayout->addStretch();
    layout->addWidget(pagerWidget);
  }

  if (!weibos.empty()) {
    for (size_t idx = start_index; idx < end_index; ++idx) {
      const WeiboData& weibo = weibos[idx];
      QWidget* cardWidget = new QWidget(m_weiboContainer);
      cardWidget->setStyleSheet(
        "QWidget { background: #313244; border-radius: 10px; border: 1px solid #45475a; } "
        "QWidget:hover { border: 1px solid #6c7086; background: #363656; }");
      QVBoxLayout* cardLayout = new QVBoxLayout(cardWidget);
      cardLayout->setContentsMargins(20, 20, 20, 20);
      cardLayout->setSpacing(14);
      
      QLabel* timeLabel = new QLabel(QString("<span style='color: #89b4fa; font-size: 12px; font-weight: bold;'>📅 %1</span>").arg(weibo.timestamp), cardWidget);
      timeLabel->setTextFormat(Qt::RichText);
      cardLayout->addWidget(timeLabel);
      
      QLabel* textLabel = new QLabel(weibo.text, cardWidget);
      textLabel->setWordWrap(true);
      textLabel->setTextFormat(Qt::PlainText);
      textLabel->setStyleSheet("color: #cdd6f4; font-size: 15px; line-height: 1.6; margin: 4px 0px;");
      textLabel->setMinimumHeight(50);
      cardLayout->addWidget(textLabel);

      // Images grid
      if (!weibo.pics.empty()) {
        QWidget* imagesWidget = new QWidget(cardWidget);
        QGridLayout* imagesLayout = new QGridLayout(imagesWidget);
        imagesLayout->setContentsMargins(0, 0, 0, 0);
        imagesLayout->setSpacing(8);
        
        int imgCount = 0;
        for (const std::string& picStr : weibo.pics) {
          QString picUrl = QString::fromStdString(picStr);
          QLabel* picLabel = new QLabel(imagesWidget);
          picLabel->setScaledContents(false);
          picLabel->setAlignment(Qt::AlignCenter);
          picLabel->setMinimumSize(120, 120);
          picLabel->setMaximumSize(300, 300);
          picLabel->setStyleSheet("border-radius: 8px; border: 2px solid #45475a; background: #1e1e2e;");
          
          if (m_imageCache.contains(picUrl)) {
            picLabel->setPixmap(m_imageCache[picUrl].scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation));
          } else {
            picLabel->setText("🖼️");
            picLabel->setStyleSheet("border-radius: 8px; border: 2px dashed #45475a; color: #6c7086; background: #1e1e2e;");
            loadImageAsync(picUrl, picLabel, 300);
          }
          imagesLayout->addWidget(picLabel, imgCount / 3, imgCount % 3);
          imgCount++;
        }
        imagesLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), (imgCount + 2) / 3, 0, 1, 3);
        cardLayout->addWidget(imagesWidget);
      }

      // Video section
      if (!weibo.video_url.empty()) {
        QString videoUrl = QString::fromStdString(weibo.video_url);
        QWidget* videoWidget = new QWidget(cardWidget);
        videoWidget->setStyleSheet(
          "QWidget { background: #1e1e2e; border-radius: 8px; border: 1px solid #45475a; } "
          "QWidget:hover { border: 1px solid #6c7086; }");
        QVBoxLayout* vLayout = new QVBoxLayout(videoWidget);
        vLayout->setContentsMargins(14, 14, 14, 14);
        vLayout->setSpacing(10);
        
        QLabel* videoIconLabel = new QLabel("<span style='color: #89b4fa; font-size: 14px; font-weight: bold;'>▶️ Video Available</span>", videoWidget);
        videoIconLabel->setTextFormat(Qt::RichText);
        vLayout->addWidget(videoIconLabel);
        
        QHBoxLayout* vcLayout = new QHBoxLayout();
        vcLayout->setContentsMargins(0, 0, 0, 0);
        vcLayout->setSpacing(8);
        
        QPushButton* playVideoBtn = new QPushButton("▶ Play in Video Tab", videoWidget);
        playVideoBtn->setStyleSheet(
          "QPushButton { background: #45475a; color: #cdd6f4; border: 1px solid #6c7086; padding: 8px 14px; border-radius: 6px; font-weight: bold; font-size: 12px; } "
          "QPushButton:hover { background: #585b70; border: 1px solid #89b4fa; } "
          "QPushButton:pressed { background: #45475a; }");
        vcLayout->addWidget(playVideoBtn);
        
        QLabel* videoStatus = new QLabel("Ready", videoWidget);
        videoStatus->setStyleSheet("color: #6c7086; font-size: 12px; font-weight: 500;");
        vcLayout->addWidget(videoStatus);
        vcLayout->addStretch();
        vLayout->addLayout(vcLayout);
        cardLayout->addWidget(videoWidget);
        
        connect(playVideoBtn, &QPushButton::clicked, [this, videoUrl, videoStatus]() {
          m_tabWidget->setCurrentIndex(2);
          m_currentVideoUrl = videoUrl;
          m_videoTabPlayer->setSource(QUrl::fromUserInput(videoUrl));
          m_videoTabStatus->setText("Loading...");
          videoStatus->setText("Loading...");
          QTimer::singleShot(100, [this, videoStatus]() { 
            m_videoTabPlayer->play();
            videoStatus->setText("Playing");
            m_videoTabStatus->setText("Playing");
          });
        });
        
      }

      layout->addWidget(cardWidget);
    }
  }

  layout->addStretch();
}

static httplib::Headers imageHeaders() {
  return {
    {"accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8"},
    {"accept-language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"},
    {"cache-control", "no-cache"},
    {"referer", "https://m.weibo.cn/"},
    {"sec-fetch-dest", "image"},
    {"sec-fetch-mode", "no-cors"},
    {"user-agent", "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Mobile Safari/537.36"}
  };
}

static std::pair<std::string, std::string> splitUrl(const std::string& url) {
  size_t pos = url.find("://");
  if (pos != std::string::npos) {
    size_t slash_pos = url.find('/', pos + 3);
    if (slash_pos != std::string::npos)
      return {url.substr(0, slash_pos), url.substr(slash_pos)};
  }
  return {url, "/"};
}

void MainWindow::loadImageAsync(const QString& picUrl, QLabel* picLabel, int maxSize) {
  QPointer<QLabel> safeLabel(picLabel);
  std::thread([this, picUrl, safeLabel, maxSize]() {
    while (m_activeDownloads.load() >= MAX_CONCURRENT_DOWNLOADS)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    m_activeDownloads++;
    try {
      auto [scheme_host, path] = splitUrl(picUrl.toStdString());
      httplib::Client cli(scheme_host);
      cli.set_follow_location(true);
      cli.set_decompress(true);
      cli.set_keep_alive(false);
      cli.set_connection_timeout(10, 0);
      auto res = cli.Get(path, imageHeaders());
      if (res && res->status == 200) {
        QPixmap pixmap;
        if (pixmap.loadFromData(reinterpret_cast<const uchar*>(res->body.c_str()), res->body.size())) {
          m_imageCache[picUrl] = pixmap;
          QMetaObject::invokeMethod(this, [safeLabel, pixmap, maxSize]() {
            if (safeLabel) {
              safeLabel->setPixmap(pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
              safeLabel->setStyleSheet("border-radius: 8px; border: 2px solid #45475a; background: #1e1e2e;");
            }
          });
        }
      }
    } catch (...) {}
    m_activeDownloads--;
  }).detach();
}

void MainWindow::downloadVideo(const QString& videoUrl, QWidget* parent) {
  QFileDialog* dialog = new QFileDialog(parent, Qt::Dialog);
  dialog->setAcceptMode(QFileDialog::AcceptSave);
  dialog->setDefaultSuffix("mp4");
  dialog->setNameFilters({"MP4 Files (*.mp4)", "All Files (*)"});
  dialog->setWindowTitle("Save Video");
  
  QString defaultName = videoUrl.section('/', -1);
  if (defaultName.contains("?")) defaultName = defaultName.section("?", 0, 0);
  if (defaultName.isEmpty() || !defaultName.contains(".")) defaultName = "video.mp4";
  dialog->selectFile(defaultName);
  
  if (dialog->exec() != QDialog::Accepted) return;
  QString savePath = dialog->selectedFiles().first();
  if (savePath.isEmpty()) return;

  const QString taskId = registerDownloadTask("video", videoUrl, savePath);
  updateDownloadTask(taskId, "Queued", 0, "Ready to download");
  
  appendLog(QString("Downloading video to %1...").arg(savePath));
  
  std::thread([this, videoUrl, savePath, taskId]() {
    m_activeDownloads++;
    QMetaObject::invokeMethod(this, [this, taskId]() {
      updateDownloadTask(taskId, "Running", 10, "Requesting video");
    }, Qt::QueuedConnection);
    try {
      auto [scheme_host, path] = splitUrl(videoUrl.toStdString());
      httplib::Client cli(scheme_host);
      cli.set_follow_location(true);
      cli.set_decompress(true);
      cli.set_keep_alive(false);
      cli.set_connection_timeout(30, 0);
      httplib::Headers headers = {
        {"accept", "*/*"},
        {"referer", "https://www.weibo.com/"},
        {"range", "bytes=0-"},
        {"sec-fetch-dest", "video"},
        {"user-agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15"}
      };
      auto res = cli.Get(path, headers);
      if (res && (res->status == 200 || res->status == 206)) {
        QMetaObject::invokeMethod(this, [this, taskId]() {
          updateDownloadTask(taskId, "Running", 60, "Writing file");
        }, Qt::QueuedConnection);
        std::ofstream outfile(savePath.toStdString(), std::ios::binary);
        if (outfile.is_open()) {
          outfile.write(res->body.c_str(), res->body.size());
          outfile.close();
          const auto savedBytes = static_cast<qulonglong>(res->body.size());
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
            Q_ARG(QString, QString("Video saved: %1 (%2 bytes)").arg(savePath).arg(res->body.size())));
          QMetaObject::invokeMethod(this, [this, taskId, savePath, savedBytes]() {
            finishDownloadTask(taskId, true,
                               QString("Saved %1 (%2 bytes)").arg(savePath).arg(savedBytes));
          }, Qt::QueuedConnection);
        } else {
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
            Q_ARG(QString, QString("Failed to open file: %1").arg(savePath)));
          QMetaObject::invokeMethod(this, [this, taskId, savePath]() {
            finishDownloadTask(taskId, false, QString("Cannot open file %1").arg(savePath));
          }, Qt::QueuedConnection);
        }
      } else {
        const int statusCode = res ? res->status : -1;
        QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
          Q_ARG(QString, QString("Failed to download video: HTTP %1").arg(statusCode)));
        QMetaObject::invokeMethod(this, [this, taskId, statusCode]() {
          finishDownloadTask(taskId, false,
                             QString("HTTP %1").arg(statusCode));
        }, Qt::QueuedConnection);
      }
    } catch (const std::exception& e) {
      QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
        Q_ARG(QString, QString("Error downloading video: %1").arg(e.what())));
      QMetaObject::invokeMethod(this, [this, taskId, e]() {
        finishDownloadTask(taskId, false, QString("Exception: %1").arg(e.what()));
      }, Qt::QueuedConnection);
    }
    m_activeDownloads--;
  }).detach();
}

void MainWindow::showAllPictures(uint64_t uid) {
  m_tabWidget->setCurrentIndex(3);
  if (!m_picTabScroll || !m_picTabContainer) return;

  QLayout* oldLayout = m_picTabContainer->layout();
  if (oldLayout) delete oldLayout;
  
  QVBoxLayout* layout = new QVBoxLayout(m_picTabContainer);
  layout->setAlignment(Qt::AlignTop);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  m_currentPictureUid = uid;
  m_currentPictureUrls.clear();

  std::vector<WeiboData> weibos;
  {
    std::lock_guard<std::mutex> lock(m_weiboMutex);
    if (m_weibos.contains(uid)) {
      weibos = m_weibos[uid];
    }
  }

  if (weibos.empty()) {
    m_picTabCountLabel->setText("Total: 0");
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 14px;'>📭 No weibo data for this user.</p>", m_picTabContainer);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noDataLabel);
    return;
  }

  QList<QString> allPictures;
  for (const WeiboData& weibo : weibos)
    for (const std::string& picStr : weibo.pics)
      allPictures.append(QString::fromStdString(picStr));

  m_currentPictureUrls = allPictures;
  m_picTabCountLabel->setText(QString("Total: %1").arg(allPictures.size()));

  if (allPictures.isEmpty()) {
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 14px;'>📭 No pictures available.</p>", m_picTabContainer);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noDataLabel);
    return;
  }

  QGridLayout* gridLayout = new QGridLayout();
  gridLayout->setSpacing(12);
  for (int idx = 0; idx < allPictures.size(); ++idx) {
    const QString& picUrl = allPictures[idx];
    QLabel* picLabel = new QLabel(m_picTabContainer);
    picLabel->setScaledContents(false);
    picLabel->setAlignment(Qt::AlignCenter);
    picLabel->setMinimumSize(150, 150);
    picLabel->setMaximumSize(400, 400);
    picLabel->setStyleSheet("border-radius: 8px; border: 2px solid #45475a; background: #1e1e2e;");
    if (m_imageCache.contains(picUrl)) {
      picLabel->setPixmap(m_imageCache[picUrl].scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
      picLabel->setText("🖼️");
      picLabel->setStyleSheet("border-radius: 8px; border: 2px dashed #45475a; color: #6c7086; background: #1e1e2e;");
      loadImageAsync(picUrl, picLabel, 400);
    }
    gridLayout->addWidget(picLabel, idx / 4, idx % 4);
  }
  gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding),
                     (allPictures.size() + 3) / 4, 0, 1, 4);
  QWidget* gridWidget = new QWidget(m_picTabContainer);
  gridWidget->setLayout(gridLayout);
  layout->addWidget(gridWidget);
  layout->addStretch();
}

void MainWindow::saveAllPictures(const QList<QString>& picUrls, QWidget* parent) {
  if (picUrls.isEmpty()) { appendLog("No pictures to save"); return; }

  QFileDialog* dialog = new QFileDialog(parent, Qt::Dialog);
  dialog->setFileMode(QFileDialog::Directory);
  dialog->setOption(QFileDialog::ShowDirsOnly);
  dialog->setWindowTitle("Select Folder to Save Pictures");
  if (dialog->exec() != QDialog::Accepted) return;
  QString saveFolderPath = dialog->selectedFiles().first();
  if (saveFolderPath.isEmpty()) return;

  const QString taskId = registerDownloadTask("pictures", "batch", saveFolderPath, picUrls.size());
  updateDownloadTask(taskId, "Queued", 0, QString("0/%1 item(s)").arg(picUrls.size()));

  appendLog(QString("Downloading %1 pictures to %2...").arg(picUrls.size()).arg(saveFolderPath));
  std::string cookies = loadCookies();
  appendLog(QString::fromStdString(fmt::format("Loaded cookies: {} bytes", cookies.size())));

  std::thread([this, picUrls, saveFolderPath, cookies, taskId]() {
    int successCount = 0, failureCount = 0;
    QMetaObject::invokeMethod(this, [this, taskId, picUrls]() {
      updateDownloadTask(taskId, "Running", 0,
                         QString("0/%1 item(s)").arg(picUrls.size()));
    }, Qt::QueuedConnection);
    for (int idx = 0; idx < picUrls.size(); ++idx) {
      try {
        auto [scheme_host, path] = splitUrl(picUrls[idx].toStdString());
        httplib::Client cli(scheme_host);
        cli.set_follow_location(true);
        cli.set_decompress(true);
        cli.set_keep_alive(false);
        cli.set_connection_timeout(10, 0);
        auto headers = imageHeaders();
        if (!cookies.empty()) headers.insert(std::make_pair("Cookie", cookies));
        auto res = cli.Get(path, headers);
        if (res && res->status == 200) {
          QString fileName = picUrls[idx].section('/', -1);
          if (fileName.contains("?")) fileName = fileName.section("?", 0, 0);
          if (fileName.isEmpty() || fileName.length() < 5) fileName = QString("picture_%1.jpg").arg(idx + 1);
          std::ofstream outfile((saveFolderPath + "/" + fileName).toStdString(), std::ios::binary);
          if (outfile.is_open()) {
            outfile.write(res->body.c_str(), res->body.size());
            outfile.close();
            successCount++;
            QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
              Q_ARG(QString, QString("Saved picture %1/%2: %3").arg(successCount + failureCount).arg(picUrls.size()).arg(fileName)));
          } else { failureCount++; }
        } else { failureCount++; }
      } catch (const std::exception& e) { failureCount++; }

      const int done = successCount + failureCount;
      const int progress = picUrls.isEmpty() ? 100 : (done * 100) / picUrls.size();
      QMetaObject::invokeMethod(this, [this, taskId, done, picUrls, successCount, failureCount, progress]() {
        updateDownloadTask(taskId,
                           "Running",
                           progress,
                           QString("%1/%2 item(s), ok=%3 fail=%4")
                               .arg(done)
                               .arg(picUrls.size())
                               .arg(successCount)
                               .arg(failureCount));
      }, Qt::QueuedConnection);

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
      Q_ARG(QString, QString("Picture download complete: %1 succeeded, %2 failed").arg(successCount).arg(failureCount)));
    QMetaObject::invokeMethod(this, [this, taskId, successCount, failureCount, picUrls]() {
      finishDownloadTask(taskId,
                         failureCount == 0,
                         QString("Done %1/%2, failed=%3")
                             .arg(successCount)
                             .arg(picUrls.size())
                             .arg(failureCount));
    }, Qt::QueuedConnection);
  }).detach();
}

void MainWindow::showAllVideos(uint64_t uid) {
  m_tabWidget->setCurrentIndex(4);
  if (!m_videoListTabScroll || !m_videoListTabContainer) return;

  QLayout* oldLayout = m_videoListTabContainer->layout();
  if (oldLayout) delete oldLayout;

  QVBoxLayout* layout = new QVBoxLayout(m_videoListTabContainer);
  layout->setAlignment(Qt::AlignTop);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(16);

  m_currentVideoListUid = uid;
  m_currentVideoUrls.clear();

  std::vector<WeiboData> weibos;
  {
    std::lock_guard<std::mutex> lock(m_weiboMutex);
    if (m_weibos.contains(uid)) {
      weibos = m_weibos[uid];
    }
  }

  if (weibos.empty()) {
    m_videoListTabCountLabel->setText("Total: 0");
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086;'>No weibo data for this user.</p>", m_videoListTabContainer);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noDataLabel);
    return;
  }

  QList<QString> allVideos;
  QMap<QString, QString> videoTimestamps, videoTexts;

  for (const WeiboData& weibo : weibos) {
    if (!weibo.video_url.empty() && weibo.video_url.find("http") == 0) {
      QString videoUrl = QString::fromStdString(weibo.video_url);
      allVideos.append(videoUrl);
      videoTimestamps[videoUrl] = weibo.timestamp;
      videoTexts[videoUrl] = weibo.text;
    }
  }

  m_currentVideoUrls = allVideos;
  m_videoListTabCountLabel->setText(QString("Total: %1").arg(allVideos.size()));

  if (allVideos.isEmpty()) {
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086;'>No videos available.</p>", m_videoListTabContainer);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noDataLabel);
    layout->addStretch();
    return;
  }

  for (const QString& videoUrl : allVideos) {
    QWidget* videoCard = new QWidget(m_videoListTabContainer);
    videoCard->setStyleSheet(
      "QWidget { background: #313244; border-radius: 10px; border: 1px solid #45475a; } "
      "QWidget:hover { border: 1px solid #cba6f7; background: #363656; }");
    QVBoxLayout* vcl = new QVBoxLayout(videoCard);
    vcl->setContentsMargins(16, 16, 16, 16);
    vcl->setSpacing(12);

    QLabel* timeLabel = new QLabel(QString("<span style='color: #cba6f7; font-size: 12px; font-weight: bold;'>📅 %1</span>").arg(videoTimestamps.value(videoUrl, "")), videoCard);
    timeLabel->setTextFormat(Qt::RichText);
    vcl->addWidget(timeLabel);

    QLabel* videoIconLabel = new QLabel("🎬 Video", videoCard);
    videoIconLabel->setStyleSheet("color: #cdd6f4; font-size: 14px; font-weight: bold;");
    videoIconLabel->setAlignment(Qt::AlignCenter);
    vcl->addWidget(videoIconLabel);

    QLabel* urlLabel = new QLabel(videoUrl.left(50) + (videoUrl.length() > 50 ? "..." : ""), videoCard);
    urlLabel->setStyleSheet("color: #6c7086; font-size: 11px;");
    urlLabel->setWordWrap(true);
    urlLabel->setAlignment(Qt::AlignCenter);
    vcl->addWidget(urlLabel);

    QString weiboText = videoTexts.value(videoUrl, "");
    if (!weiboText.isEmpty()) {
      QLabel* textLabel = new QLabel(weiboText.left(200) + (weiboText.length() > 200 ? "..." : ""), videoCard);
      textLabel->setWordWrap(true);
      textLabel->setTextFormat(Qt::PlainText);
      textLabel->setStyleSheet("color: #cdd6f4; font-size: 13px;");
      textLabel->setMinimumHeight(50);
      vcl->addWidget(textLabel);
    }

    QPushButton* playBtn = new QPushButton("▶ Play in Video Tab", videoCard);
    playBtn->setStyleSheet(
      "QPushButton { background: #cba6f7; color: #1e1e2e; border: none; padding: 10px 20px; border-radius: 6px; font-weight: bold; } "
      "QPushButton:hover { background: #f5c2e7; }");
    connect(playBtn, &QPushButton::clicked, [this, videoUrl]() {
      m_tabWidget->setCurrentIndex(2);
      m_currentVideoUrl = videoUrl;
      m_videoTabPlayer->setSource(QUrl::fromUserInput(videoUrl));
      m_videoTabStatus->setText("Loading...");
      QTimer::singleShot(100, [this]() {
        m_videoTabPlayer->play();
        m_videoTabStatus->setText("Playing");
      });
    });
    vcl->addWidget(playBtn);
    layout->addWidget(videoCard);
  }

  layout->addStretch();
}

void MainWindow::setupDownloadManagerTab() {
  QWidget* downloadsTabContent = new QWidget(this);
  QVBoxLayout* downloadsLayout = new QVBoxLayout(downloadsTabContent);
  downloadsLayout->setContentsMargins(16, 16, 16, 16);
  downloadsLayout->setSpacing(10);

  QLabel* title = new QLabel("Download Manager", downloadsTabContent);
  title->setStyleSheet("font-size: 18px; font-weight: 700;");
  downloadsLayout->addWidget(title);

  QLabel* hint = new QLabel("Track single video and batch picture downloads.", downloadsTabContent);
  hint->setStyleSheet("color: #6c7086;");
  downloadsLayout->addWidget(hint);

  QWidget* actions = new QWidget(downloadsTabContent);
  QHBoxLayout* actionsLayout = new QHBoxLayout(actions);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(8);

  QPushButton* clearFinishedBtn = new QPushButton("Clear Finished", actions);
  connect(clearFinishedBtn, &QPushButton::clicked, [this]() {
    if (!m_downloadTable) {
      return;
    }
    for (int row = m_downloadTable->rowCount() - 1; row >= 0; --row) {
      QTableWidgetItem* statusItem = m_downloadTable->item(row, 3);
      if (!statusItem) {
        continue;
      }
      const QString status = statusItem->text();
      if (status == "Completed" || status == "Failed") {
        const QString taskId = m_downloadTable->item(row, 0)->text();
        m_downloadRowById.remove(taskId);
        m_downloadTable->removeRow(row);
      }
    }
  });
  actionsLayout->addWidget(clearFinishedBtn);

  QPushButton* clearAllBtn = new QPushButton("Clear All", actions);
  connect(clearAllBtn, &QPushButton::clicked, [this]() {
    if (!m_downloadTable) {
      return;
    }
    m_downloadTable->setRowCount(0);
    m_downloadRowById.clear();
  });
  actionsLayout->addWidget(clearAllBtn);

  actionsLayout->addStretch();
  downloadsLayout->addWidget(actions);

  m_downloadTable = new QTableWidget(downloadsTabContent);
  m_downloadTable->setColumnCount(7);
  m_downloadTable->setHorizontalHeaderLabels({
      "Task", "Type", "Target", "Status", "Progress", "Detail", "Updated"});
  m_downloadTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_downloadTable->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_downloadTable->setSelectionMode(QAbstractItemView::SingleSelection);
  m_downloadTable->verticalHeader()->setVisible(false);
  m_downloadTable->horizontalHeader()->setStretchLastSection(true);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
  m_downloadTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
  downloadsLayout->addWidget(m_downloadTable);

  m_tabWidget->addTab(downloadsTabContent, "⬇ Downloads");
}

QString MainWindow::registerDownloadTask(const QString& type,
                                         const QString& source,
                                         const QString& target,
                                         int total_items) {
  Q_UNUSED(source);

  const uint64_t seq = ++m_downloadTaskSeq;
  const QString taskId = QString("DL-%1").arg(seq, 4, 10, QLatin1Char('0'));

  if (!m_downloadTable) {
    return taskId;
  }

  const int row = m_downloadTable->rowCount();
  m_downloadTable->insertRow(row);
  m_downloadTable->setItem(row, 0, new QTableWidgetItem(taskId));
  m_downloadTable->setItem(row, 1, new QTableWidgetItem(type));
  m_downloadTable->setItem(row, 2, new QTableWidgetItem(target));
  m_downloadTable->setItem(row, 3, new QTableWidgetItem("Queued"));
  m_downloadTable->setItem(row, 4, new QTableWidgetItem("0%"));
  m_downloadTable->setItem(row, 5,
                           new QTableWidgetItem(QString("0/%1 item(s)").arg(std::max(1, total_items))));
  m_downloadTable->setItem(row, 6,
                           new QTableWidgetItem(QDateTime::currentDateTime().toString("hh:mm:ss")));

  m_downloadRowById[taskId] = row;
  return taskId;
}

void MainWindow::updateDownloadTask(const QString& task_id,
                                    const QString& status,
                                    int progress,
                                    const QString& detail) {
  if (!m_downloadTable || !m_downloadRowById.contains(task_id)) {
    return;
  }

  const int row = m_downloadRowById[task_id];
  progress = std::max(0, std::min(100, progress));

  if (QTableWidgetItem* statusItem = m_downloadTable->item(row, 3)) {
    statusItem->setText(status);
  }
  if (QTableWidgetItem* progressItem = m_downloadTable->item(row, 4)) {
    progressItem->setText(QString("%1%").arg(progress));
  }
  if (QTableWidgetItem* detailItem = m_downloadTable->item(row, 5)) {
    detailItem->setText(detail);
  }
  if (QTableWidgetItem* updatedItem = m_downloadTable->item(row, 6)) {
    updatedItem->setText(QDateTime::currentDateTime().toString("hh:mm:ss"));
  }

  if (m_tabWidget) {
    int downloadTabIndex = m_tabWidget->indexOf(m_downloadTable->parentWidget());
    if (downloadTabIndex >= 0 && (status == "Failed" || status == "Completed")) {
      m_tabWidget->setTabText(downloadTabIndex, QString("⬇ Downloads*"));
    }
  }
}

void MainWindow::finishDownloadTask(const QString& task_id,
                                    bool success,
                                    const QString& detail) {
  updateDownloadTask(task_id, success ? "Completed" : "Failed", success ? 100 : 0, detail);
}
