#include "mainwindow.hpp"
#include "spider.hpp"
#include "weibo.hpp"
#include <QBoxLayout>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QThread>
#include <QTimer>
#include <QMessageBox>
#include <thread>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QPen>
#include <QBrush>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <fstream>
#include <fmt/core.h>
#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_startBtn(new QPushButton("▶ Start", this))
    , m_stopBtn(new QPushButton("■ Stop", this))
    , m_logEdit(new QTextEdit(this))
    , m_graphView(new ZoomGraphicsView(this))
    , m_graphScene(new QGraphicsScene(this))
    , m_crawlWeiboCheck(new QCheckBox("Weibo", this))
    , m_crawlFansCheck(new QCheckBox("Fans", this))
    , m_crawlFollowersCheck(new QCheckBox("Followers", this))
    , m_playVideoCheck(new QCheckBox("▶ Play MP4", this))
    , m_uidInput(new QLineEdit(this))
    , m_themeCombo(new QComboBox(this))
    , m_layoutCombo(new QComboBox(this))
    , m_applyLayoutBtn(new QPushButton("⟳ Apply", this))
    , m_tabWidget(new QTabWidget(this))
    , m_running(false)
    , m_crawlWeibo(true)
    , m_targetUid(6126303533)
    , m_nodeCount(0)
    , m_currentTheme(0)
    , m_videoWidget(nullptr)
    , m_weiboScroll(nullptr)
    , m_weiboContainer(nullptr) {
  m_imageClient = std::make_unique<httplib::Client>("https://weibo.com");
  m_imageClient->set_follow_location(true);
  m_imageClient->set_decompress(true);
  m_videoPlayer = std::make_unique<QMediaPlayer>();
  initThemes();
  loadConfig();
  setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::initThemes() {
  m_themes = {
    {"Ocean", QColor("#00D4FF"), QColor("#0077B6"), QColor("#00F5D4"), QColor("#FF6B6B"), QColor("#E8F4F8"), QColor("#023E8A")},
    {"Midnight", QColor("#7C3AED"), QColor("#4C1D95"), QColor("#A78BFA"), QColor("#F472B6"), QColor("#0F172A"), QColor("#CBD5E1")},
    {"Aurora", QColor("#10B981"), QColor("#047857"), QColor("#6EE7B7"), QColor("#F43F5E"), QColor("#ECFDF5"), QColor("#064E3B")},
    {"Coral", QColor("#F97316"), QColor("#C2410C"), QColor("#FDBA74"), QColor("#FB7185"), QColor("#FFF7ED"), QColor("#7C2D12")},
    {"Neon", QColor("#E879F9"), QColor("#A21CAF"), QColor("#D8B4FE"), QColor("#22D3EE"), QColor("#1E1B4B"), QColor("#E9D5FF")},
    {"Slate", QColor("#3B82F6"), QColor("#1D4ED8"), QColor("#60A5FA"), QColor("#F87171"), QColor("#0F172A"), QColor("#F1F5F9")}
  };
}

void MainWindow::applyTheme(int index) {
  if (index < 0 || index >= m_themes.size()) return;
  m_currentTheme = index;
  const Theme& theme = m_themes[index];

  m_graphScene->setBackgroundBrush(QBrush(theme.background));
  
  for (auto* node : m_nodes.values()) {
    node->setTheme(theme);
    node->updateLines();
  }

  for (auto it = m_lines.constBegin(); it != m_lines.constEnd(); ++it) {
    QPen pen = it.value()->pen();
    bool isFollower = m_lineIsFollower.value(it.key(), true);
    pen.setColor(isFollower ? theme.followerLine : theme.fanLine);
    it.value()->setPen(pen);
  }
}

void NodeItem::updateLines() {
  QPointF start = pos();
  for (const auto& pair : m_lines) {
    QGraphicsPathItem* line = pair.first;
    NodeItem* other = pair.second;
    QPointF endPos = other->pos();
    
    QPainterPath path;
    path.moveTo(start);
    QPointF ctrl1 = QPointF(start.x() + (endPos.x() - start.x()) * 0.5, start.y() - 30);
    QPointF ctrl2 = QPointF(start.x() + (endPos.x() - start.x()) * 0.5, endPos.y() - 30);
    path.cubicTo(ctrl1, ctrl2, endPos);
    line->setPath(path);
  }
}

void MainWindow::setupUi() {
  setWindowTitle("Weibo Spider");
  resize(1400, 900);

  m_logEdit->setReadOnly(true);
  m_logEdit->setMaximumHeight(120);
  m_logEdit->setPlaceholderText("Logs will appear here...");
  m_stopBtn->setEnabled(false);

  m_graphView->setScene(m_graphScene);
  m_graphView->setRenderHint(QPainter::Antialiasing);
  m_graphView->setDragMode(QGraphicsView::ScrollHandDrag);
  m_graphScene->setSceneRect(-2000, -2000, 4000, 4000);
  m_graphView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  QToolBar* toolbar = addToolBar("Main Toolbar");
  toolbar->setMovable(false);
  toolbar->setFloatable(false);
  toolbar->setIconSize(QSize(20, 20));
  toolbar->setStyleSheet("QToolBar { background: #181825; border: none; padding: 8px; spacing: 8px; }");

  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  
  toolbar->addWidget(m_startBtn);
  toolbar->addWidget(m_stopBtn);
  toolbar->addSeparator();
  
  QLabel* uidLabel = new QLabel("UID:", this);
  uidLabel->setStyleSheet("color: #a6adc8; font-weight: 500;");
  toolbar->addWidget(uidLabel);
  
  m_uidInput->setPlaceholderText("Enter UID");
  m_uidInput->setText(QString::number(m_targetUid));
  m_uidInput->setFixedWidth(120);
  m_uidInput->setStyleSheet("background: #313244; color: #cdd6f4; border: 1px solid #45475a; border-radius: 4px; padding: 4px 8px;");
  toolbar->addWidget(m_uidInput);
  
  m_crawlWeiboCheck->setChecked(m_crawlWeibo);
  m_crawlFansCheck->setChecked(true);
  m_crawlFollowersCheck->setChecked(true);
  toolbar->addWidget(m_crawlWeiboCheck);
  toolbar->addWidget(m_crawlFansCheck);
  toolbar->addWidget(m_crawlFollowersCheck);
  toolbar->addSeparator();
  
  m_playVideoCheck->setChecked(true);
  toolbar->addWidget(m_playVideoCheck);
  toolbar->addSeparator();
  toolbar->addWidget(spacer);

  QLabel* themeLabel = new QLabel("🎨 Theme:", this);
  themeLabel->setStyleSheet("color: #a6adc8; font-weight: 500;");
  toolbar->addWidget(themeLabel);
  toolbar->addWidget(m_themeCombo);

  QLabel* layoutLabel = new QLabel("📊 Layout:", this);
  layoutLabel->setStyleSheet("color: #a6adc8; font-weight: 500; margin-left: 16px;");
  toolbar->addWidget(layoutLabel);
  toolbar->addWidget(m_layoutCombo);
  toolbar->addWidget(m_applyLayoutBtn);
  
  QLabel* zoomLabel = new QLabel("🔍 Zoom:", this);
  zoomLabel->setStyleSheet("color: #a6adc8; font-weight: 500; margin-left: 16px;");
  toolbar->addWidget(zoomLabel);
  
  QPushButton* zoomInBtn = new QPushButton("+", this);
  zoomInBtn->setFixedWidth(32);
  zoomInBtn->setToolTip("Zoom In (Ctrl++)");
  connect(zoomInBtn, &QPushButton::clicked, [this]() { m_graphView->scale(1.15, 1.15); });
  toolbar->addWidget(zoomInBtn);
  
  QPushButton* zoomOutBtn = new QPushButton("-", this);
  zoomOutBtn->setFixedWidth(32);
  zoomOutBtn->setToolTip("Zoom Out (Ctrl+-)");
  connect(zoomOutBtn, &QPushButton::clicked, [this]() { m_graphView->scale(1.0/1.15, 1.0/1.15); });
  toolbar->addWidget(zoomOutBtn);
  
  QPushButton* zoomResetBtn = new QPushButton("⟲", this);
  zoomResetBtn->setFixedWidth(32);
  zoomResetBtn->setToolTip("Reset Zoom (Ctrl+0)");
  connect(zoomResetBtn, &QPushButton::clicked, [this]() { m_graphView->resetTransform(); });
  toolbar->addWidget(zoomResetBtn);

  for (const Theme& t : m_themes) {
    m_themeCombo->addItem(t.name);
  }
  connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::applyTheme);

  m_layoutCombo->addItem("Random");
  m_layoutCombo->addItem("Circular");
  m_layoutCombo->addItem("Force Directed");
  m_layoutCombo->addItem("Kamada-Kawai");
  m_layoutCombo->addItem("Grid");
  m_layoutCombo->addItem("Hierarchical");
  m_layoutCombo->setCurrentIndex(1);
  m_currentLayout = LayoutType::Circular;
  connect(m_layoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &MainWindow::onLayoutChanged);
  connect(m_applyLayoutBtn, &QPushButton::clicked, this, &MainWindow::onApplyLayoutClicked);

  QWidget* centralWidget = new QWidget(this);
  QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(12, 8, 12, 12);
  mainLayout->setSpacing(8);

  QLabel* logLabel = new QLabel("📜 Logs", this);
  logLabel->setStyleSheet("color: #a6adc8; font-weight: 600; font-size: 14px; margin-bottom: 4px;");
  mainLayout->addWidget(logLabel);
  mainLayout->addWidget(m_logEdit);

  m_tabWidget->setStyleSheet(R"(
    QTabWidget::pane { border: 1px solid #313244; border-radius: 8px; background: #181825; }
    QTabBar::tab { background: #313244; color: #cdd6f4; padding: 8px 16px; border-top-left-radius: 6px; border-top-right-radius: 6px; }
    QTabBar::tab:selected { background: #45475a; }
    QTabBar::tab:hover { background: #45475a; }
  )");
  
  m_graphView->setStyleSheet("background: #181825; border: none; border-radius: 8px;");
  m_tabWidget->addTab(m_graphView, "🌐 Graph");
  
  QWidget* weiboTabContent = new QWidget(this);
  weiboTabContent->setStyleSheet("background: #181825;");
  QVBoxLayout* weiboTabLayout = new QVBoxLayout(weiboTabContent);
  weiboTabLayout->setContentsMargins(0, 0, 0, 0);
  weiboTabLayout->setSpacing(0);
  
  m_videoPlayerWidget = new QWidget(this);
  m_videoPlayerWidget->setStyleSheet("background: #1e1e2e;");
  QVBoxLayout* videoLayout = new QVBoxLayout(m_videoPlayerWidget);
  videoLayout->setContentsMargins(8, 8, 8, 8);
  videoLayout->setSpacing(4);
  m_videoPlayerWidget->hide();
  
  m_videoWidget = new QVideoWidget(this);
  m_videoWidget->setMinimumSize(320, 200);
  m_videoWidget->setMaximumHeight(250);
  m_videoWidget->setStyleSheet("background: #000; border-radius: 8px;");
  videoLayout->addWidget(m_videoWidget);
  
  QWidget* videoControls = new QWidget(this);
  videoControls->setStyleSheet("background: #313244; border-radius: 4px;");
  QHBoxLayout* controlsLayout = new QHBoxLayout(videoControls);
  controlsLayout->setContentsMargins(8, 4, 8, 4);
  
  QPushButton* playBtn = new QPushButton("▶ Play", this);
  playBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 6px 12px; border-radius: 4px;");
  connect(playBtn, &QPushButton::clicked, [this]() {
    if (m_videoPlayer->playbackState() == QMediaPlayer::PlayingState) {
      m_videoPlayer->pause();
    } else {
      m_videoPlayer->play();
    }
  });
  controlsLayout->addWidget(playBtn);
  
  QPushButton* stopBtn = new QPushButton("■ Stop", this);
  stopBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 6px 12px; border-radius: 4px;");
  connect(stopBtn, &QPushButton::clicked, [this]() {
    m_videoPlayer->stop();
  });
  controlsLayout->addWidget(stopBtn);
  
  QLabel* videoStatus = new QLabel("No video loaded", this);
  videoStatus->setStyleSheet("color: #6c7086;");
  controlsLayout->addWidget(videoStatus);
  controlsLayout->addStretch();
  
  videoLayout->addWidget(videoControls);
  weiboTabLayout->addWidget(m_videoPlayerWidget);
  
  QWidget* statsBar = new QWidget(this);
  statsBar->setStyleSheet("background: #1e1e2e;");
  QHBoxLayout* statsLayout = new QHBoxLayout(statsBar);
  statsLayout->setContentsMargins(16, 8, 16, 8);
  
  m_totalWeiboLabel = new QLabel("Total Weibo: 0", this);
  m_totalWeiboLabel->setStyleSheet("color: #cdd6f4; font-size: 14px;");
  statsLayout->addWidget(m_totalWeiboLabel);
  
  m_totalVideoLabel = new QLabel("Total Video: 0", this);
  m_totalVideoLabel->setStyleSheet("color: #cba6f7; font-size: 14px;");
  statsLayout->addWidget(m_totalVideoLabel);
  
  statsLayout->addStretch();
  weiboTabLayout->addWidget(statsBar);
  
  m_videoPlayer->setVideoOutput(m_videoWidget);
  
   QScrollArea* weiboScroll = new QScrollArea(this);
   weiboScroll->setStyleSheet(
     "QScrollArea { "
     "  background: #181825; "
     "  border: none; "
     "} "
     "QScrollBar:vertical { "
     "  width: 8px; "
     "  background: #1e1e2e; "
     "} "
     "QScrollBar::handle:vertical { "
     "  background: #45475a; "
     "  border-radius: 4px; "
     "  min-height: 20px; "
     "} "
     "QScrollBar::handle:vertical:hover { "
     "  background: #585b70; "
     "}"
   );
   weiboScroll->setWidgetResizable(true);
   
   QWidget* weiboContainer = new QWidget(this);
   weiboContainer->setStyleSheet("background: #181825;");
   QVBoxLayout* weiboLayout = new QVBoxLayout(weiboContainer);
   weiboLayout->setAlignment(Qt::AlignTop);
   weiboLayout->setContentsMargins(24, 24, 24, 24);
   weiboLayout->setSpacing(20);
  
  m_weiboScroll = weiboScroll;
  m_weiboContainer = weiboContainer;
  weiboScroll->setWidget(weiboContainer);
  weiboTabLayout->addWidget(weiboScroll);
  
  m_tabWidget->addTab(weiboTabContent, "📝 Weibo");
  
  QWidget* videoTabContent = new QWidget(this);
  videoTabContent->setStyleSheet("background: #181825;");
  QVBoxLayout* videoTabLayout = new QVBoxLayout(videoTabContent);
  videoTabLayout->setContentsMargins(16, 16, 16, 16);
  videoTabLayout->setSpacing(16);
  
  m_videoTabWidget = new QVideoWidget(videoTabContent);
  m_videoTabWidget->setMinimumSize(640, 360);
  m_videoTabWidget->setStyleSheet("background: #000; border-radius: 8px;");
  m_videoTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  videoTabLayout->addWidget(m_videoTabWidget);
  
  m_videoTabPlayer = std::make_unique<QMediaPlayer>();
  m_videoTabPlayer->setVideoOutput(m_videoTabWidget);
  m_videoTabAudio = std::make_unique<QAudioOutput>();
  m_videoTabAudio->setVolume(1.0);
  m_videoTabPlayer->setAudioOutput(m_videoTabAudio.get());
  
  QWidget* videoTabControls = new QWidget(videoTabContent);
  videoTabControls->setStyleSheet("background: #313244; border-radius: 4px;");
  QHBoxLayout* videoTabControlsLayout = new QHBoxLayout(videoTabControls);
  videoTabControlsLayout->setContentsMargins(12, 8, 12, 8);
  
  QPushButton* videoTabPlayBtn = new QPushButton("▶ Play", videoTabControls);
  videoTabPlayBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(videoTabPlayBtn, &QPushButton::clicked, [this, videoTabPlayBtn]() {
    if (m_videoTabPlayer->playbackState() == QMediaPlayer::PlayingState) {
      m_videoTabPlayer->pause();
      videoTabPlayBtn->setText("▶ Play");
    } else {
      m_videoTabPlayer->play();
      videoTabPlayBtn->setText("■ Pause");
    }
  });
  videoTabControlsLayout->addWidget(videoTabPlayBtn);
  
  QPushButton* videoTabStopBtn = new QPushButton("■ Stop", videoTabControls);
  videoTabStopBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(videoTabStopBtn, &QPushButton::clicked, [this, videoTabPlayBtn]() {
    m_videoTabPlayer->stop();
    videoTabPlayBtn->setText("▶ Play");
  });
  videoTabControlsLayout->addWidget(videoTabStopBtn);
  
  QPushButton* videoTabSaveBtn = new QPushButton("💾 Save", videoTabControls);
  videoTabSaveBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(videoTabSaveBtn, &QPushButton::clicked, [this]() {
    if (m_currentVideoUrl.isEmpty()) {
      m_videoTabStatus->setText("No video to save");
      return;
    }
    downloadVideo(m_currentVideoUrl, this);
  });
  videoTabControlsLayout->addWidget(videoTabSaveBtn);
  
  m_videoTabStatus = new QLabel("No video loaded", videoTabControls);
  m_videoTabStatus->setStyleSheet("color: #6c7086; font-size: 14px;");
  videoTabControlsLayout->addWidget(m_videoTabStatus);
  videoTabControlsLayout->addStretch();
  
  videoTabLayout->addWidget(videoTabControls);
  
  connect(m_videoTabPlayer.get(), &QMediaPlayer::playbackStateChanged, [videoTabPlayBtn](QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState) {
      videoTabPlayBtn->setText("■ Pause");
    } else if (state == QMediaPlayer::StoppedState) {
      videoTabPlayBtn->setText("▶ Play");
    } else if (state == QMediaPlayer::PausedState) {
      videoTabPlayBtn->setText("▶ Play");
    }
  });
  
   connect(m_videoTabPlayer.get(), &QMediaPlayer::errorOccurred, [this](QMediaPlayer::Error error, const QString& errorString) {
     m_videoTabStatus->setText(QString("Error: %1").arg(errorString));
   });
   
   m_tabWidget->addTab(videoTabContent, "🎬 Video");
   
   // Picture Tab
   QWidget* picTabContent = new QWidget(this);
   picTabContent->setStyleSheet("background: #181825;");
   QVBoxLayout* picTabLayout = new QVBoxLayout(picTabContent);
   picTabLayout->setContentsMargins(0, 0, 0, 0);
   picTabLayout->setSpacing(0);
   
   // Picture Tab Controls
   QWidget* picTabControls = new QWidget(this);
   picTabControls->setStyleSheet("background: #313244; border-radius: 4px;");
   QHBoxLayout* picTabControlsLayout = new QHBoxLayout(picTabControls);
   picTabControlsLayout->setContentsMargins(12, 8, 12, 8);
   picTabControlsLayout->setSpacing(8);
   
   QLabel* picTabLabel = new QLabel("📸 All Pictures", picTabControls);
   picTabLabel->setStyleSheet("color: #cdd6f4; font-size: 14px; font-weight: bold;");
   picTabControlsLayout->addWidget(picTabLabel);
   
   QPushButton* picTabSaveBtn = new QPushButton("💾 Save All", picTabControls);
   picTabSaveBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
   connect(picTabSaveBtn, &QPushButton::clicked, [this]() {
     if (m_currentPictureUrls.isEmpty()) {
       QMessageBox::information(this, "No Pictures", "No pictures available to save.");
       return;
     }
     saveAllPictures(m_currentPictureUrls, this);
   });
   picTabControlsLayout->addWidget(picTabSaveBtn);
   
   QLabel* picCountLabel = new QLabel("Total: 0", picTabControls);
   picCountLabel->setStyleSheet("color: #6c7086; font-size: 12px;");
   picTabControlsLayout->addWidget(picCountLabel);
   
   picTabControlsLayout->addStretch();
   picTabLayout->addWidget(picTabControls);
   
   // Picture Tab Scroll Area
   QScrollArea* picTabScroll = new QScrollArea(this);
   picTabScroll->setStyleSheet(
     "QScrollArea { "
     "  background: #181825; "
     "  border: none; "
     "} "
     "QScrollBar:vertical { "
     "  width: 8px; "
     "  background: #1e1e2e; "
     "} "
     "QScrollBar::handle:vertical { "
     "  background: #45475a; "
     "  border-radius: 4px; "
     "  min-height: 20px; "
     "} "
     "QScrollBar::handle:vertical:hover { "
     "  background: #585b70; "
     "}"
   );
   picTabScroll->setWidgetResizable(true);
   
   QWidget* picTabContainer = new QWidget(this);
   picTabContainer->setStyleSheet("background: #181825;");
   QGridLayout* picGridLayout = new QGridLayout(picTabContainer);
   picGridLayout->setAlignment(Qt::AlignTop);
   picGridLayout->setContentsMargins(16, 16, 16, 16);
   picGridLayout->setSpacing(12);
   
   m_picTabScroll = picTabScroll;
   m_picTabContainer = picTabContainer;
   picTabScroll->setWidget(picTabContainer);
   picTabLayout->addWidget(picTabScroll);
   
   m_tabWidget->addTab(picTabContent, "🖼️ Pictures");
   
   mainLayout->addWidget(m_tabWidget, 1);

  setCentralWidget(centralWidget);

  connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
  connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
}

void MainWindow::onStartClicked() {
  m_running = true;
  m_startBtn->setEnabled(false);
  m_stopBtn->setEnabled(true);
  m_logEdit->append("Starting spider...");
  runSpider();
}

void MainWindow::onStopClicked() {
  m_running = false;
  if (m_spider) {
    m_spider->stop();
  }
  m_startBtn->setEnabled(true);
  m_stopBtn->setEnabled(false);
  m_logEdit->append("Spider stopped.");
}

void MainWindow::appendLog(const QString& message) {
  m_logEdit->append(message);
}

void MainWindow::onUserFetched(uint64_t uid, const QString& name, const QList<uint64_t>& followers, const QList<uint64_t>& fans) {
  addUserNode(uid, name, followers, fans);
}

void MainWindow::onWeiboFetched(uint64_t uid, const QString& weiboText) {
  WeiboData data;
  data.text = weiboText;
  m_weibos[uid].push_back(data);
}

QPointF MainWindow::getRandomPosition() {
  double angle = QRandomGenerator::global()->bounded(360.0);
  double radius = 100.0 + m_nodeCount * 30.0;
  double x = radius * qCos(angle * M_PI / 180.0);
  double y = radius * qSin(angle * M_PI / 180.0);
  return QPointF(x, y);
}

void MainWindow::addUserNode(uint64_t uid, const QString& name, const QList<uint64_t>& followers, const QList<uint64_t>& fans) {
  if (m_nodes.contains(uid)) {
    return;
  }

  QPointF pos = getRandomPosition();
  m_positions[uid] = pos;
  m_nodeCount++;

  NodeItem* node = new NodeItem(-20, -20, 40, 40);
  node->setPos(pos);
  node->setUid(uid);
  node->setClickCallback([this](uint64_t clickedUid) {
    QMetaObject::invokeMethod(this, "showNodeWeibo", Qt::QueuedConnection, Q_ARG(uint64_t, clickedUid));
  });
  m_graphScene->addItem(node);
  m_nodes[uid] = node;

  QGraphicsTextItem* label = new QGraphicsTextItem(name.isEmpty() ? QString::number(uid) : name);
  const Theme& theme = m_themes[m_currentTheme];
  label->setDefaultTextColor(theme.text);
  label->setPos(pos.x() - 30, pos.y() + 25);
  m_graphScene->addItem(label);
  m_labels[uid] = label;
  node->setLabel(label);

  node->setTheme(theme);

  for (uint64_t follower_id : followers) {
    m_adjacency[uid].push_back(follower_id);
    m_adjacency[follower_id].push_back(uid);
    if (m_nodes.contains(follower_id)) {
      NodeItem* otherNode = m_nodes[follower_id];
      QPointF endPos = otherNode->pos();
      QPainterPath path;
      path.moveTo(pos);
      QPointF ctrl1 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, pos.y() - 30);
      QPointF ctrl2 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, endPos.y() - 30);
      path.cubicTo(ctrl1, ctrl2, endPos);
      QGraphicsPathItem* line = new QGraphicsPathItem(path);
      line->setPen(QPen(theme.followerLine, 1.5));
      m_graphScene->addItem(line);
      uint64_t lineKey = uid * 1000000 + follower_id;
      m_lines[lineKey] = line;
      m_lineIsFollower[lineKey] = true;
      node->addLine(line, otherNode);
      otherNode->addLine(line, node);
    }
  }

  for (uint64_t fan_id : fans) {
    if (m_nodes.contains(fan_id)) {
      NodeItem* otherNode = m_nodes[fan_id];
      QPointF endPos = otherNode->pos();
      QPainterPath path;
      path.moveTo(pos);
      QPointF ctrl1 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, pos.y() + 30);
      QPointF ctrl2 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, endPos.y() + 30);
      path.cubicTo(ctrl1, ctrl2, endPos);
      QGraphicsPathItem* line = new QGraphicsPathItem(path);
      line->setPen(QPen(theme.fanLine, 1.5));
      m_graphScene->addItem(line);
      uint64_t lineKey = uid * 1000000 + fan_id;
      m_lines[lineKey] = line;
      m_lineIsFollower[lineKey] = false;
      node->addLine(line, otherNode);
      otherNode->addLine(line, node);
    }
  }

  QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
                            Q_ARG(QString, QString("Added node: %1 (%2) followers:%3 fans:%4")
                                  .arg(name).arg(uid).arg(followers.size()).arg(fans.size())));
}

void MainWindow::runSpider() {
  m_crawlWeibo = m_crawlWeiboCheck->isChecked();
  m_targetUid = m_uidInput->text().toULongLong();
  bool crawlFans = m_crawlFansCheck->isChecked();
  bool crawlFollowers = m_crawlFollowersCheck->isChecked();
  saveConfig();

  QThread* thread = QThread::create([this, crawlFans, crawlFollowers]() {
    try {
      m_spider = std::make_unique<Spider>(m_targetUid);
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
            if (!weibo.video_url.empty() && weibo.video_url.find("http") == 0) {
              totalVideo++;
            }
          }
        }
        QMetaObject::invokeMethod(this, "updateWeiboStats", Qt::QueuedConnection,
                                  Q_ARG(int, totalWeibo),
                                  Q_ARG(int, totalVideo));
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
  
  // Header with user info
  QString title = m_labels.contains(uid) ? m_labels[uid]->toPlainText() : QString::number(uid);
  QWidget* headerWidget = new QWidget(m_weiboContainer);
  headerWidget->setStyleSheet(
    "QWidget { "
    "  background: linear-gradient(135deg, #45475a 0%, #313244 100%); "
    "  border-radius: 10px; "
    "} "
  );
  QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
  headerLayout->setContentsMargins(20, 20, 20, 20);
  headerLayout->setSpacing(12);
  
  QLabel* titleLabel = new QLabel(QString("<span style='font-size: 20px; font-weight: bold; color: #89b4fa;'>👤 %1</span>").arg(title), headerWidget);
  titleLabel->setTextFormat(Qt::RichText);
  headerLayout->addWidget(titleLabel);
  
   QLabel* uidLabel = new QLabel(QString("<span style='color: #6c7086; font-size: 13px;'>ID: %1</span>").arg(uid), headerWidget);
   uidLabel->setTextFormat(Qt::RichText);
   headerLayout->addWidget(uidLabel);
   
   QHBoxLayout* buttonLayout = new QHBoxLayout();
   buttonLayout->setContentsMargins(0, 8, 0, 0);
   buttonLayout->setSpacing(8);
   
   QPushButton* viewPicBtn = new QPushButton("🖼️ View All Pictures", headerWidget);
   viewPicBtn->setStyleSheet(
     "QPushButton { "
     "  background: #45475a; "
     "  color: #cdd6f4; "
     "  border: 1px solid #6c7086; "
     "  padding: 8px 14px; "
     "  border-radius: 6px; "
     "  font-weight: bold; "
     "  font-size: 12px; "
     "} "
     "QPushButton:hover { "
     "  background: #585b70; "
     "  border: 1px solid #89b4fa; "
     "} "
     "QPushButton:pressed { "
     "  background: #45475a; "
     "}"
   );
   connect(viewPicBtn, &QPushButton::clicked, [this, uid]() {
     QMetaObject::invokeMethod(this, "showAllPictures", Qt::QueuedConnection, Q_ARG(uint64_t, uid));
   });
   buttonLayout->addWidget(viewPicBtn);
   buttonLayout->addStretch();
   
   headerLayout->addLayout(buttonLayout);
   
   layout->addWidget(headerWidget);
  
  if (m_weibos.contains(uid)) {
    const auto& weibos = m_weibos[uid];
    if (weibos.empty()) {
      QWidget* emptyWidget = new QWidget(m_weiboContainer);
      emptyWidget->setStyleSheet("background: #313244; border-radius: 8px;");
      QVBoxLayout* emptyLayout = new QVBoxLayout(emptyWidget);
      emptyLayout->setContentsMargins(32, 32, 32, 32);
      QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 14px;'>📭 No weibo data available.</p>", emptyWidget);
      noDataLabel->setTextFormat(Qt::RichText);
      noDataLabel->setAlignment(Qt::AlignCenter);
      emptyLayout->addWidget(noDataLabel);
      layout->addWidget(emptyWidget);
    } else {
      for (const WeiboData& weibo : weibos) {
        // Card container
        QWidget* cardWidget = new QWidget(m_weiboContainer);
        cardWidget->setStyleSheet(
          "QWidget { "
          "  background: #313244; "
          "  border-radius: 10px; "
          "  border: 1px solid #45475a; "
          "} "
          "QWidget:hover { "
          "  border: 1px solid #6c7086; "
          "  background: #363656; "
          "}"
        );
        QVBoxLayout* cardLayout = new QVBoxLayout(cardWidget);
        cardLayout->setContentsMargins(20, 20, 20, 20);
        cardLayout->setSpacing(14);
        
        // Timestamp header
        QLabel* timeLabel = new QLabel(QString("<span style='color: #89b4fa; font-size: 12px; font-weight: bold;'>📅 %1</span>").arg(weibo.timestamp), cardWidget);
        timeLabel->setTextFormat(Qt::RichText);
        cardLayout->addWidget(timeLabel);
        
        // Weibo text
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
              QPixmap pixmap = m_imageCache[picUrl].scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
              picLabel->setPixmap(pixmap);
            } else {
              picLabel->setText("🖼️");
              picLabel->setStyleSheet("border-radius: 8px; border: 2px dashed #45475a; color: #6c7086; background: #1e1e2e;");
              
              std::thread([this, picUrl, picLabel, imagesWidget]() {
                std::string url = picUrl.toStdString();
                size_t pos = url.find("://");
                std::string scheme_host;
                std::string path;
                if (pos != std::string::npos) {
                  size_t slash_pos = url.find('/', pos + 3);
                  if (slash_pos != std::string::npos) {
                    scheme_host = url.substr(0, slash_pos);
                    path = url.substr(slash_pos);
                  }
                }
                
                if (scheme_host.empty()) {
                  scheme_host = url;
                  path = "/";
                }
                
                httplib::Client cli(scheme_host);
                cli.set_follow_location(true);
                cli.set_decompress(true);
                
                httplib::Headers headers = {
                  {"accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8"},
                  {"accept-language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"},
                  {"cache-control", "no-cache"},
                  {"pragma", "no-cache"},
                  {"priority", "i"},
                  {"referer", "https://m.weibo.cn/"},
                  {"sec-ch-ua", "\"Not(A:Brand\";v=\"99\", \"Google Chrome\";v=\"133\", \"Chromium\";v=\"133\""},
                  {"sec-ch-ua-mobile", "?1"},
                  {"sec-ch-ua-platform", "\"Android\""},
                  {"sec-fetch-dest", "image"},
                  {"sec-fetch-mode", "no-cors"},
                  {"sec-fetch-site", "cross-site"},
                  {"sec-fetch-storage-access", "active"},
                  {"user-agent", "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Mobile Safari/537.36"}
                };
                
                auto res = cli.Get(path, headers);
                if (res && res->status == 200) {
                  QPixmap pixmap;
                  if (pixmap.loadFromData(reinterpret_cast<const uchar*>(res->body.c_str()), res->body.size())) {
                    m_imageCache[picUrl] = pixmap;
                    QMetaObject::invokeMethod(this, [this, picUrl, picLabel, pixmap]() {
                      if (picLabel) {
                        QPixmap scaled = pixmap.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        picLabel->setPixmap(scaled);
                        picLabel->setStyleSheet("border-radius: 8px; border: 2px solid #45475a; background: #1e1e2e;");
                      }
                    });
                  }
                }
              }).detach();
            }
            
            int row = imgCount / 3;
            int col = imgCount % 3;
            imagesLayout->addWidget(picLabel, row, col);
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
            "QWidget { "
            "  background: #1e1e2e; "
            "  border-radius: 8px; "
            "  border: 1px solid #45475a; "
            "} "
            "QWidget:hover { "
            "  border: 1px solid #6c7086; "
            "}"
          );
          QVBoxLayout* videoLayout = new QVBoxLayout(videoWidget);
          videoLayout->setContentsMargins(14, 14, 14, 14);
          videoLayout->setSpacing(10);
          
          QLabel* videoIconLabel = new QLabel("<span style='color: #89b4fa; font-size: 14px; font-weight: bold;'>▶️ Video Available</span>", videoWidget);
          videoIconLabel->setTextFormat(Qt::RichText);
          videoLayout->addWidget(videoIconLabel);
          
          QHBoxLayout* videoControlsLayout = new QHBoxLayout();
          videoControlsLayout->setContentsMargins(0, 0, 0, 0);
          videoControlsLayout->setSpacing(8);
          
          QPushButton* playVideoBtn = new QPushButton("▶ Play in Video Tab", videoWidget);
          playVideoBtn->setStyleSheet(
            "QPushButton { "
            "  background: #45475a; "
            "  color: #cdd6f4; "
            "  border: 1px solid #6c7086; "
            "  padding: 8px 14px; "
            "  border-radius: 6px; "
            "  font-weight: bold; "
            "  font-size: 12px; "
            "} "
            "QPushButton:hover { "
            "  background: #585b70; "
            "  border: 1px solid #89b4fa; "
            "} "
            "QPushButton:pressed { "
            "  background: #45475a; "
            "}"
          );
          videoControlsLayout->addWidget(playVideoBtn);
          
          QLabel* videoStatus = new QLabel("Ready", videoWidget);
          videoStatus->setStyleSheet("color: #6c7086; font-size: 12px; font-weight: 500;");
          videoControlsLayout->addWidget(videoStatus);
          videoControlsLayout->addStretch();
          
          videoLayout->addLayout(videoControlsLayout);
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
            if (state == QMediaPlayer::PlayingState) {
              videoStatus->setText("Playing");
            } else if (state == QMediaPlayer::StoppedState) {
              videoStatus->setText("Ready");
            } else if (state == QMediaPlayer::PausedState) {
              videoStatus->setText("Paused");
            }
          });
          
          connect(m_videoTabPlayer.get(), &QMediaPlayer::mediaStatusChanged, [videoStatus](QMediaPlayer::MediaStatus status) {
            switch (status) {
              case QMediaPlayer::LoadedMedia:
                videoStatus->setText("Ready");
                break;
              case QMediaPlayer::LoadingMedia:
                videoStatus->setText("Loading...");
                break;
              case QMediaPlayer::BufferingMedia:
                videoStatus->setText("Buffering...");
                break;
              case QMediaPlayer::StalledMedia:
                videoStatus->setText("Stalled");
                break;
              case QMediaPlayer::InvalidMedia:
                videoStatus->setText("Error: Invalid");
                break;
              default:
                break;
            }
          });
          
          connect(m_videoTabPlayer.get(), &QMediaPlayer::errorOccurred, [videoStatus](QMediaPlayer::Error error, const QString& errorString) {
            videoStatus->setText(QString("Error: %1").arg(errorString));
          });
        }
        
        layout->addWidget(cardWidget);
      }
    }
  } else {
    QWidget* emptyWidget = new QWidget(m_weiboContainer);
    emptyWidget->setStyleSheet(
      "QWidget { "
      "  background: #313244; "
      "  border-radius: 10px; "
      "  border: 2px dashed #45475a; "
      "}"
    );
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setContentsMargins(40, 60, 40, 60);
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 15px; line-height: 1.6;'>📭<br/>No weibo data available.<br/><span style='font-size: 13px;'>Enable Weibo crawling and run the spider.</span></p>", emptyWidget);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(noDataLabel);
    layout->addWidget(emptyWidget);
  }
  
  layout->addStretch();
}

void MainWindow::loadConfig() {
  QFile file("config.json");
  if (file.open(QIODevice::ReadOnly)) {
    QByteArray data = file.readAll();
    file.close();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull() && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("crawl_weibo")) {
        m_crawlWeibo = obj["crawl_weibo"].toBool(true);
      }
      if (obj.contains("target_uid")) {
        m_targetUid = obj["target_uid"].toVariant().toULongLong();
      }
    }
  }
}

void MainWindow::saveConfig() {
  QJsonObject obj;
  obj["crawl_weibo"] = m_crawlWeiboCheck->isChecked();
  obj["target_uid"] = QString::number(m_targetUid);
  QJsonDocument doc(obj);
  QFile file("config.json");
  if (file.open(QIODevice::WriteOnly)) {
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
  }
}

void MainWindow::onLayoutChanged(int index) {
  switch (index) {
    case 0: m_currentLayout = LayoutType::Random; break;
    case 1: m_currentLayout = LayoutType::Circular; break;
    case 2: m_currentLayout = LayoutType::ForceDirected; break;
    case 3: m_currentLayout = LayoutType::KamadaKawai; break;
    case 4: m_currentLayout = LayoutType::Grid; break;
    case 5: m_currentLayout = LayoutType::Hierarchical; break;
    default: m_currentLayout = LayoutType::Circular; break;
  }
  applyLayout(m_currentLayout);
}

void MainWindow::onApplyLayoutClicked() {
  applyLayout(m_currentLayout);
}

void MainWindow::applyLayout(LayoutType type) {
  if (m_nodes.isEmpty()) return;

  int width = 1000;
  int height = 800;
  if (m_nodeCount > 50) {
    width = m_nodeCount * 15;
    height = m_nodeCount * 15;
  }

  QMap<uint64_t, QPointF> positions;
  for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
    positions[it.key()] = it.value()->pos();
  }

  QMap<uint64_t, QPointF> newPositions = GraphLayout::applyLayout(
      type, positions, m_adjacency, width, height);

  updateNodePositions(newPositions);

  QRectF bounds = m_graphScene->itemsBoundingRect();
  m_graphScene->setSceneRect(bounds.adjusted(-100, -100, 100, 100));
  m_graphView->centerOn(bounds.center());
}

void MainWindow::updateNodePositions(const QMap<uint64_t, QPointF>& newPositions) {
  for (auto it = newPositions.constBegin(); it != newPositions.constEnd(); ++it) {
    uint64_t uid = it.key();
    if (m_nodes.contains(uid)) {
      NodeItem* node = m_nodes[uid];
      node->setPos(it.value());
      m_positions[uid] = it.value();

      if (m_labels.contains(uid)) {
        m_labels[uid]->setPos(it.value().x() - 30, it.value().y() + 25);
      }

      for (const auto& pair : node->m_lines) {
        QGraphicsPathItem* line = pair.first;
        NodeItem* other = pair.second;
        QPointF start = node->pos();
        QPointF endPos = other->pos();
        
        QPainterPath path;
        path.moveTo(start);
        
        uint64_t otherUid = other->uid();
        uint64_t lineKey = (uid < otherUid ? uid : otherUid) * 1000000 + (uid < otherUid ? otherUid : uid);
        bool isFollower = m_lineIsFollower.value(lineKey, true);
        
        QPointF ctrl1 = QPointF(start.x() + (endPos.x() - start.x()) * 0.5, 
                               start.y() + (isFollower ? -30 : 30));
        QPointF ctrl2 = QPointF(start.x() + (endPos.x() - start.x()) * 0.5, 
                               endPos.y() + (isFollower ? -30 : 30));
        path.cubicTo(ctrl1, ctrl2, endPos);
        line->setPath(path);
      }
    }
  }
}

void MainWindow::downloadVideo(const QString& videoUrl, QWidget* parent) {
  QFileDialog* dialog = new QFileDialog(parent, Qt::Dialog);
  dialog->setAcceptMode(QFileDialog::AcceptSave);
  dialog->setDefaultSuffix("mp4");
  dialog->setNameFilters({"MP4 Files (*.mp4)", "All Files (*)"});
  dialog->setWindowTitle("Save Video");
  
  QString defaultName = videoUrl.section('/', -1);
  if (defaultName.contains("?")) {
    defaultName = defaultName.section("?", 0, 0);
  }
  if (defaultName.isEmpty() || !defaultName.contains(".")) {
    defaultName = "video.mp4";
  }
  dialog->selectFile(defaultName);
  
  if (dialog->exec() != QDialog::Accepted) {
    return;
  }
  
  QString savePath = dialog->selectedFiles().first();
  if (savePath.isEmpty()) {
    return;
  }
  
  appendLog(QString("Downloading video to %1...").arg(savePath));
  
  std::thread([this, videoUrl, savePath]() {
    try {
      std::string url = videoUrl.toStdString();
      size_t pos = url.find("://");
      std::string scheme_host;
      std::string path;
      if (pos != std::string::npos) {
        size_t slash_pos = url.find('/', pos + 3);
        if (slash_pos != std::string::npos) {
          scheme_host = url.substr(0, slash_pos);
          path = url.substr(slash_pos);
        }
      }
      
      if (scheme_host.empty()) {
        scheme_host = url;
        path = "/";
      }
      
      httplib::Client cli(scheme_host);
      cli.set_follow_location(true);
      cli.set_decompress(true);
      
      httplib::Headers headers = {
        {"accept", "*/*"},
        {"accept-language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"},
        {"cache-control", "no-cache"},
        {"pragma", "no-cache"},
        {"priority", "i"},
        {"range", "bytes=0-"},
        {"referer", "https://www.weibo.com/"},
        {"sec-fetch-dest", "video"},
        {"sec-fetch-mode", "no-cors"},
        {"sec-fetch-site", "cross-site"},
        {"sec-fetch-storage-access", "active"},
        {"user-agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/16.0 Safari/605.1.15"}
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
            Q_ARG(QString, QString("Failed to open file for writing: %1").arg(savePath)));
        }
      } else {
        QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
          Q_ARG(QString, QString("Failed to download video: HTTP %1").arg(res ? res->status : -1)));
      }
    } catch (const std::exception& e) {
      QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
        Q_ARG(QString, QString("Error downloading video: %1").arg(e.what())));
    }
  }).detach();
}

void MainWindow::saveAllPictures(const QList<QString>& picUrls, QWidget* parent) {
  if (picUrls.isEmpty()) {
    appendLog("No pictures to save");
    return;
  }

  QFileDialog* dialog = new QFileDialog(parent, Qt::Dialog);
  dialog->setFileMode(QFileDialog::Directory);
  dialog->setOption(QFileDialog::ShowDirsOnly);
  dialog->setWindowTitle("Select Folder to Save Pictures");

  if (dialog->exec() != QDialog::Accepted) {
    return;
  }

  QString saveFolderPath = dialog->selectedFiles().first();
  if (saveFolderPath.isEmpty()) {
    return;
  }

  appendLog(QString("Downloading %1 pictures to %2...").arg(picUrls.size()).arg(saveFolderPath));

  std::thread([this, picUrls, saveFolderPath]() {
    int successCount = 0;
    int failureCount = 0;

    for (int idx = 0; idx < picUrls.size(); ++idx) {
      const QString& picUrl = picUrls[idx];

      try {
        std::string url = picUrl.toStdString();
        size_t pos = url.find("://");
        std::string scheme_host;
        std::string path;
        if (pos != std::string::npos) {
          size_t slash_pos = url.find('/', pos + 3);
          if (slash_pos != std::string::npos) {
            scheme_host = url.substr(0, slash_pos);
            path = url.substr(slash_pos);
          }
        }

        if (scheme_host.empty()) {
          scheme_host = url;
          path = "/";
        }

        httplib::Client cli(scheme_host);
        cli.set_follow_location(true);
        cli.set_decompress(true);

        httplib::Headers headers = {
          {"accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8"},
          {"accept-language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"},
          {"cache-control", "no-cache"},
          {"pragma", "no-cache"},
          {"priority", "i"},
          {"referer", "https://m.weibo.cn/"},
          {"sec-ch-ua", "\"Not(A:Brand\";v=\"99\", \"Google Chrome\";v=\"133\", \"Chromium\";v=\"133\""},
          {"sec-ch-ua-mobile", "?1"},
          {"sec-ch-ua-platform", "\"Android\""},
          {"sec-fetch-dest", "image"},
          {"sec-fetch-mode", "no-cors"},
          {"sec-fetch-site", "cross-site"},
          {"sec-fetch-storage-access", "active"},
          {"user-agent", "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Mobile Safari/537.36"}
        };

        auto res = cli.Get(path, headers);
        if (res && res->status == 200) {
          // Generate filename from URL or index
          QString fileName = picUrl.section('/', -1);
          if (fileName.contains("?")) {
            fileName = fileName.section("?", 0, 0);
          }
          if (fileName.isEmpty() || fileName.length() < 5) {
            fileName = QString("picture_%1.jpg").arg(idx + 1);
          }

          QString fullPath = saveFolderPath + "/" + fileName;
          std::ofstream outfile(fullPath.toStdString(), std::ios::binary);
          if (outfile.is_open()) {
            outfile.write(res->body.c_str(), res->body.size());
            outfile.close();
            successCount++;
            QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
              Q_ARG(QString, QString("Saved picture %1/%2: %3").arg(successCount + failureCount).arg(picUrls.size()).arg(fileName)));
          } else {
            failureCount++;
            QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
              Q_ARG(QString, QString("Failed to save picture %1: file write error").arg(idx + 1)));
          }
        } else {
          failureCount++;
          QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
            Q_ARG(QString, QString("Failed to download picture %1: HTTP %2").arg(idx + 1).arg(res ? res->status : -1)));
        }
      } catch (const std::exception& e) {
        failureCount++;
        QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
          Q_ARG(QString, QString("Error downloading picture %1: %2").arg(idx + 1).arg(e.what())));
      }

      // Small delay between downloads to avoid overwhelming the server
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    QMetaObject::invokeMethod(this, "appendLog", Qt::QueuedConnection,
      Q_ARG(QString, QString("Picture download complete: %1 succeeded, %2 failed").arg(successCount).arg(failureCount)));
  }).detach();
}

void MainWindow::showAllPictures(uint64_t uid) {
  m_tabWidget->setCurrentIndex(3);  // Switch to picture tab (index 3: Graph=0, Weibo=1, Video=2, Pictures=3)
  
  if (!m_picTabScroll || !m_picTabContainer) return;

  // Delete the old layout completely and create a new one
  QLayout* oldLayout = m_picTabContainer->layout();
  if (oldLayout) {
    delete oldLayout;
  }
  
  QVBoxLayout* layout = new QVBoxLayout(m_picTabContainer);
  layout->setAlignment(Qt::AlignTop);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSpacing(12);

  m_currentPictureUid = uid;
  m_currentPictureUrls.clear();

  if (m_weibos.contains(uid)) {
    const auto& weibos = m_weibos[uid];
    
    // Collect all pictures from all weibos
    QList<QString> allPictures;
    for (const WeiboData& weibo : weibos) {
      for (const std::string& picStr : weibo.pics) {
        allPictures.append(QString::fromStdString(picStr));
      }
    }

    m_currentPictureUrls = allPictures;

    if (allPictures.isEmpty()) {
      QWidget* emptyWidget = new QWidget(m_picTabContainer);
      emptyWidget->setStyleSheet("background: #313244; border-radius: 8px;");
      QVBoxLayout* emptyLayout = new QVBoxLayout(emptyWidget);
      emptyLayout->setContentsMargins(32, 32, 32, 32);
      QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 14px;'>📭 No pictures available.</p>", emptyWidget);
      noDataLabel->setTextFormat(Qt::RichText);
      noDataLabel->setAlignment(Qt::AlignCenter);
      emptyLayout->addWidget(noDataLabel);
      layout->addWidget(emptyWidget);
    } else {
      // Create grid layout for pictures
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
          QPixmap pixmap = m_imageCache[picUrl].scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
          picLabel->setPixmap(pixmap);
        } else {
          picLabel->setText("🖼️");
          picLabel->setStyleSheet("border-radius: 8px; border: 2px dashed #45475a; color: #6c7086; background: #1e1e2e;");

          // Download picture asynchronously
          std::thread([this, picUrl, picLabel]() {
            std::string url = picUrl.toStdString();
            size_t pos = url.find("://");
            std::string scheme_host;
            std::string path;
            if (pos != std::string::npos) {
              size_t slash_pos = url.find('/', pos + 3);
              if (slash_pos != std::string::npos) {
                scheme_host = url.substr(0, slash_pos);
                path = url.substr(slash_pos);
              }
            }

            if (scheme_host.empty()) {
              scheme_host = url;
              path = "/";
            }

            httplib::Client cli(scheme_host);
            cli.set_follow_location(true);
            cli.set_decompress(true);

            httplib::Headers headers = {
              {"accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8"},
              {"accept-language", "en-US,en;q=0.9,zh-CN;q=0.8,zh;q=0.7"},
              {"cache-control", "no-cache"},
              {"pragma", "no-cache"},
              {"priority", "i"},
              {"referer", "https://m.weibo.cn/"},
              {"sec-ch-ua", "\"Not(A:Brand\";v=\"99\", \"Google Chrome\";v=\"133\", \"Chromium\";v=\"133\""},
              {"sec-ch-ua-mobile", "?1"},
              {"sec-ch-ua-platform", "\"Android\""},
              {"sec-fetch-dest", "image"},
              {"sec-fetch-mode", "no-cors"},
              {"sec-fetch-site", "cross-site"},
              {"sec-fetch-storage-access", "active"},
              {"user-agent", "Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Mobile Safari/537.36"}
            };

            auto res = cli.Get(path, headers);
            if (res && res->status == 200) {
              QPixmap pixmap;
              if (pixmap.loadFromData(reinterpret_cast<const uchar*>(res->body.c_str()), res->body.size())) {
                m_imageCache[picUrl] = pixmap;
                QMetaObject::invokeMethod(this, [this, picUrl, picLabel, pixmap]() {
                  if (picLabel) {
                    QPixmap scaled = pixmap.scaled(400, 400, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    picLabel->setPixmap(scaled);
                    picLabel->setStyleSheet("border-radius: 8px; border: 2px solid #45475a; background: #1e1e2e;");
                  }
                });
              }
            }
          }).detach();
        }

        int row = idx / 4;  // 4 columns for picture grid
        int col = idx % 4;
        gridLayout->addWidget(picLabel, row, col);
      }

      // Add stretch at the end
      gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Expanding), 
                         (allPictures.size() + 3) / 4, 0, 1, 4);

      QWidget* gridWidget = new QWidget(m_picTabContainer);
      gridWidget->setLayout(gridLayout);
      layout->addWidget(gridWidget);
    }

    layout->addStretch();
  } else {
    QWidget* emptyWidget = new QWidget(m_picTabContainer);
    emptyWidget->setStyleSheet("background: #313244; border-radius: 8px;");
    QVBoxLayout* emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setContentsMargins(32, 32, 32, 32);
    QLabel* noDataLabel = new QLabel("<p style='color: #6c7086; text-align: center; font-size: 14px;'>📭 No weibo data for this user.</p>", emptyWidget);
    noDataLabel->setTextFormat(Qt::RichText);
    noDataLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(noDataLabel);
    layout->addWidget(emptyWidget);
  }
}
