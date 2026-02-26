#include "mainwindow.hpp"
#include <QBoxLayout>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>
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

void MainWindow::showNodeWeibo(uint64_t uid) {
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

  if (m_weibos.contains(uid) && !m_weibos[uid].empty()) {
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

  if (m_weibos.contains(uid) && !m_weibos[uid].empty()) {
    const auto& weibos = m_weibos[uid];
    for (const WeiboData& weibo : weibos) {
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
        
        connect(m_videoTabPlayer.get(), &QMediaPlayer::playbackStateChanged, [videoStatus](QMediaPlayer::PlaybackState state) {
          if (state == QMediaPlayer::PlayingState) videoStatus->setText("Playing");
          else if (state == QMediaPlayer::StoppedState) videoStatus->setText("Ready");
          else if (state == QMediaPlayer::PausedState) videoStatus->setText("Paused");
        });
        connect(m_videoTabPlayer.get(), &QMediaPlayer::mediaStatusChanged, [videoStatus](QMediaPlayer::MediaStatus status) {
          switch (status) {
            case QMediaPlayer::LoadedMedia: videoStatus->setText("Ready"); break;
            case QMediaPlayer::LoadingMedia: videoStatus->setText("Loading..."); break;
            case QMediaPlayer::BufferingMedia: videoStatus->setText("Buffering..."); break;
            case QMediaPlayer::StalledMedia: videoStatus->setText("Stalled"); break;
            case QMediaPlayer::InvalidMedia: videoStatus->setText("Error: Invalid"); break;
            default: break;
          }
        });
        connect(m_videoTabPlayer.get(), &QMediaPlayer::errorOccurred, [videoStatus](QMediaPlayer::Error error, const QString& errorString) {
          videoStatus->setText(QString("Error: %1").arg(errorString));
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
  std::thread([this, picUrl, picLabel, maxSize]() {
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
          QMetaObject::invokeMethod(this, [picLabel, pixmap, maxSize]() {
            if (picLabel) {
              picLabel->setPixmap(pixmap.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
              picLabel->setStyleSheet("border-radius: 8px; border: 2px solid #45475a; background: #1e1e2e;");
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
  
  appendLog(QString("Downloading video to %1...").arg(savePath));
  
  std::thread([this, videoUrl, savePath]() {
    m_activeDownloads++;
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
        std::ofstream outfile(savePath.toStdString(), std::ios::binary);
        if (outfile.is_open()) {
          outfile.write(res->body.c_str(), res->body.size());
          outfile.close();
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
            Q_ARG(QString, QString("Video saved: %1 (%2 bytes)").arg(savePath).arg(res->body.size())));
        } else {
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
            Q_ARG(QString, QString("Failed to open file: %1").arg(savePath)));
        }
      } else {
        QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
          Q_ARG(QString, QString("Failed to download video: HTTP %1").arg(res ? res->status : -1)));
      }
    } catch (const std::exception& e) {
      QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
        Q_ARG(QString, QString("Error downloading video: %1").arg(e.what())));
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

  if (!m_weibos.contains(uid)) {
    m_picTabCountLabel->setText("Total: 0");
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 14px;'>📭 No weibo data for this user.</p>", m_picTabContainer);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noDataLabel);
    return;
  }

  QList<QString> allPictures;
  for (const WeiboData& weibo : m_weibos[uid])
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

  appendLog(QString("Downloading %1 pictures to %2...").arg(picUrls.size()).arg(saveFolderPath));
  std::string cookies = loadCookies();
  appendLog(QString::fromStdString(fmt::format("Loaded cookies: {} bytes", cookies.size())));

  std::thread([this, picUrls, saveFolderPath, cookies]() {
    int successCount = 0, failureCount = 0;
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
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
      Q_ARG(QString, QString("Picture download complete: %1 succeeded, %2 failed").arg(successCount).arg(failureCount)));
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

  if (!m_weibos.contains(uid)) {
    m_videoListTabCountLabel->setText("Total: 0");
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086;'>No weibo data for this user.</p>", m_videoListTabContainer);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noDataLabel);
    return;
  }

  QList<QString> allVideos;
  QMap<QString, QString> videoTimestamps, videoTexts;

  for (const WeiboData& weibo : m_weibos[uid]) {
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
