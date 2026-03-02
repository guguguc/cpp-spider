#include "mainwindow.hpp"
#include <QBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QScrollArea>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QTimer>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QMessageBox>

void MainWindow::applyTheme(int index) {
  if (index < 0 || index >= m_themes.size()) return;
  m_currentTheme = index;
  const Theme& theme = m_themes[index];

  m_graphScene->setBackgroundBrush(QBrush(theme.background));

  const bool dark_mode = theme.background.lightness() < 128;
  const QColor window_bg = dark_mode ? theme.background.darker(150) : theme.background.lighter(103);
  const QColor panel_bg = dark_mode ? theme.background.lighter(118) : theme.background.darker(104);
  const QColor card_bg = dark_mode ? theme.background.lighter(132) : theme.background.darker(110);
  const QColor border = dark_mode ? theme.nodePen.lighter(135) : theme.nodePen.darker(110);
  const QColor accent = theme.nodeBrush;
  const QColor accent_hover = dark_mode ? accent.lighter(118) : accent.darker(106);
  const QColor text_primary = theme.text;
  const QColor text_muted = dark_mode ? theme.text.darker(140) : theme.text.lighter(145);

  const QString style = QString(R"(
    QMainWindow, QWidget { background-color: %1; color: %2; }
    QToolBar { background: %3; border: 1px solid %4; border-radius: 8px; padding: 8px; spacing: 8px; }
    QLabel { color: %2; }
    QLineEdit, QComboBox, QTextEdit { background: %5; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 8px; selection-background-color: %6; selection-color: %1; }
    QComboBox::drop-down { border: none; width: 18px; }
    QCheckBox { spacing: 6px; color: %2; }
    QCheckBox::indicator { width: 14px; height: 14px; border-radius: 4px; border: 1px solid %4; background: %5; }
    QCheckBox::indicator:checked { background: %6; border: 1px solid %6; }
    QPushButton { background: %6; color: %1; border: 1px solid %6; border-radius: 6px; padding: 6px 12px; font-weight: 600; }
    QPushButton:hover { background: %7; border: 1px solid %7; }
    QPushButton:disabled { background: %3; border: 1px solid %4; color: %8; }
    QTabWidget::pane { border: 1px solid %4; border-radius: 10px; background: %3; }
    QTabBar::tab { background: %5; color: %2; padding: 8px 16px; border-top-left-radius: 6px; border-top-right-radius: 6px; }
    QTabBar::tab:selected { background: %6; color: %1; }
    QScrollArea { border: none; background: %3; }
    QScrollBar:vertical { width: 8px; background: %5; margin: 2px; border-radius: 4px; }
    QScrollBar::handle:vertical { background: %4; min-height: 24px; border-radius: 4px; }
    QScrollBar::handle:vertical:hover { background: %6; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
  )")
    .arg(window_bg.name()).arg(text_primary.name()).arg(panel_bg.name()).arg(border.name())
    .arg(card_bg.name()).arg(accent.name()).arg(accent_hover.name()).arg(text_muted.name());
  setStyleSheet(style);

  m_graphView->setStyleSheet(QString("background: %1; border: 1px solid %2; border-radius: 10px;")
      .arg(panel_bg.name()).arg(border.name()));
  m_logPanel->applyThemeColors(window_bg, panel_bg, card_bg, border,
                               text_primary, text_muted, accent, dark_mode);

  for (auto* node : m_nodes.values()) { node->setTheme(theme); node->updateLines(); }
  for (auto* label : m_labels.values()) { label->setDefaultTextColor(theme.text); }
  for (auto it = m_lines.constBegin(); it != m_lines.constEnd(); ++it) {
    QPen pen = it.value()->pen();
    pen.setColor(m_lineIsFollower.value(it.key(), true) ? theme.followerLine : theme.fanLine);
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

  QWidget* spacer = new QWidget();
  spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  
  toolbar->addWidget(m_startBtn);
  toolbar->addWidget(m_stopBtn);
  toolbar->addSeparator();
  
  QLabel* uidLabel = new QLabel("UID:", this);
  toolbar->addWidget(uidLabel);
  
  m_uidInput->setPlaceholderText("Enter UID");
  m_uidInput->setText(QString::number(m_targetUid));
  m_uidInput->setFixedWidth(120);
  toolbar->addWidget(m_uidInput);
  
  m_crawlWeiboCheck->setChecked(m_crawlWeibo);
  m_crawlFansCheck->setChecked(true);
  m_crawlFollowersCheck->setChecked(true);
  toolbar->addWidget(m_crawlWeiboCheck);
  toolbar->addWidget(m_crawlFansCheck);
  toolbar->addWidget(m_crawlFollowersCheck);

  QLabel* depthLabel = new QLabel("Depth:", this);
  depthLabel->setStyleSheet("margin-left: 8px;");
  toolbar->addWidget(depthLabel);
  m_depthSpin->setRange(0, 5);
  m_depthSpin->setValue(m_appConfig.crawl_max_depth);
  m_depthSpin->setToolTip("Recursive crawl depth (0 = only target user)");
  m_depthSpin->setFixedWidth(60);
  toolbar->addWidget(m_depthSpin);
  toolbar->addSeparator();
  
  m_playVideoCheck->setChecked(true);
  toolbar->addWidget(m_playVideoCheck);
  toolbar->addSeparator();
  toolbar->addWidget(spacer);

  QLabel* themeLabel = new QLabel("🎨 Theme:", this);
  toolbar->addWidget(themeLabel);
  toolbar->addWidget(m_themeCombo);

  QLabel* layoutLabel = new QLabel("📊 Layout:", this);
  layoutLabel->setStyleSheet("margin-left: 16px;");
  toolbar->addWidget(layoutLabel);
  toolbar->addWidget(m_layoutCombo);
  toolbar->addWidget(m_applyLayoutBtn);
  
  QLabel* zoomLabel = new QLabel("🔍 Zoom:", this);
  zoomLabel->setStyleSheet("margin-left: 16px;");
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

  for (const Theme& t : m_themes) { m_themeCombo->addItem(t.name); }
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

  m_tabWidget->setStyleSheet(R"(
    QTabWidget::pane { border-radius: 8px; }
    QTabBar::tab { padding: 8px 16px; border-top-left-radius: 6px; border-top-right-radius: 6px; }
  )");

  m_tabWidget->addTab(m_graphView, "🌐 Graph");
  
  // --- Weibo Tab ---
  QWidget* weiboTabContent = new QWidget(this);
  weiboTabContent->setStyleSheet("background: #181825;");
  QVBoxLayout* weiboTabLayout = new QVBoxLayout(weiboTabContent);
  weiboTabLayout->setContentsMargins(0, 0, 0, 0);
  weiboTabLayout->setSpacing(0);
  
  m_videoPlayerWidget = new QWidget(this);
  m_videoPlayerWidget->setStyleSheet("background: #1e1e2e;");
  QVBoxLayout* vpLayout = new QVBoxLayout(m_videoPlayerWidget);
  vpLayout->setContentsMargins(8, 8, 8, 8);
  vpLayout->setSpacing(4);
  m_videoPlayerWidget->hide();
  
  m_videoWidget = new QVideoWidget(this);
  m_videoWidget->setMinimumSize(320, 200);
  m_videoWidget->setMaximumHeight(250);
  m_videoWidget->setStyleSheet("background: #000; border-radius: 8px;");
  vpLayout->addWidget(m_videoWidget);
  
  QWidget* videoControls = new QWidget(this);
  videoControls->setStyleSheet("background: #313244; border-radius: 4px;");
  QHBoxLayout* controlsLayout = new QHBoxLayout(videoControls);
  controlsLayout->setContentsMargins(8, 4, 8, 4);
  
  QPushButton* playBtn = new QPushButton("▶ Play", this);
  playBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 6px 12px; border-radius: 4px;");
  connect(playBtn, &QPushButton::clicked, [this]() {
    if (m_videoPlayer->playbackState() == QMediaPlayer::PlayingState) m_videoPlayer->pause();
    else m_videoPlayer->play();
  });
  controlsLayout->addWidget(playBtn);
  
  QPushButton* vstopBtn = new QPushButton("■ Stop", this);
  vstopBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 6px 12px; border-radius: 4px;");
  connect(vstopBtn, &QPushButton::clicked, [this]() { m_videoPlayer->stop(); });
  controlsLayout->addWidget(vstopBtn);
  
  QLabel* videoStatus = new QLabel("No video loaded", this);
  videoStatus->setStyleSheet("color: #6c7086;");
  controlsLayout->addWidget(videoStatus);
  controlsLayout->addStretch();
  
  vpLayout->addWidget(videoControls);
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
    "QScrollArea { background: #181825; border: none; } "
    "QScrollBar:vertical { width: 8px; background: #1e1e2e; } "
    "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; min-height: 20px; } "
    "QScrollBar::handle:vertical:hover { background: #585b70; }");
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

  // --- Video Tab ---
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
  QHBoxLayout* vtcLayout = new QHBoxLayout(videoTabControls);
  vtcLayout->setContentsMargins(12, 8, 12, 8);
  
  QPushButton* videoTabPlayBtn = new QPushButton("▶ Play", videoTabControls);
  videoTabPlayBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(videoTabPlayBtn, &QPushButton::clicked, [this, videoTabPlayBtn]() {
    if (m_videoTabPlayer->playbackState() == QMediaPlayer::PlayingState) {
      m_videoTabPlayer->pause(); videoTabPlayBtn->setText("▶ Play");
    } else {
      m_videoTabPlayer->play(); videoTabPlayBtn->setText("■ Pause");
    }
  });
  vtcLayout->addWidget(videoTabPlayBtn);
  
  QPushButton* videoTabStopBtn = new QPushButton("■ Stop", videoTabControls);
  videoTabStopBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(videoTabStopBtn, &QPushButton::clicked, [this, videoTabPlayBtn]() {
    m_videoTabPlayer->stop(); videoTabPlayBtn->setText("▶ Play");
  });
  vtcLayout->addWidget(videoTabStopBtn);
  
  QPushButton* videoTabSaveBtn = new QPushButton("💾 Save", videoTabControls);
  videoTabSaveBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(videoTabSaveBtn, &QPushButton::clicked, [this]() {
    if (m_currentVideoUrl.isEmpty()) { m_videoTabStatus->setText("No video to save"); return; }
    downloadVideo(m_currentVideoUrl, this);
  });
  vtcLayout->addWidget(videoTabSaveBtn);
  
  m_videoTabStatus = new QLabel("No video loaded", videoTabControls);
  m_videoTabStatus->setStyleSheet("color: #6c7086; font-size: 14px;");
  vtcLayout->addWidget(m_videoTabStatus);
  vtcLayout->addStretch();
  
  videoTabLayout->addWidget(videoTabControls);
  
  connect(m_videoTabPlayer.get(), &QMediaPlayer::playbackStateChanged, [videoTabPlayBtn](QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState) videoTabPlayBtn->setText("■ Pause");
    else videoTabPlayBtn->setText("▶ Play");
  });
  connect(m_videoTabPlayer.get(), &QMediaPlayer::errorOccurred, [this](QMediaPlayer::Error error, const QString& errorString) {
    m_videoTabStatus->setText(QString("Error: %1").arg(errorString));
  });
  
  m_tabWidget->addTab(videoTabContent, "🎬 Video");

  // --- Picture Tab ---
  QWidget* picTabContent = new QWidget(this);
  picTabContent->setStyleSheet("background: #181825;");
  QVBoxLayout* picTabLayout = new QVBoxLayout(picTabContent);
  picTabLayout->setContentsMargins(0, 0, 0, 0);
  picTabLayout->setSpacing(0);
  
  QWidget* picTabControls = new QWidget(this);
  picTabControls->setStyleSheet("background: #313244; border-radius: 4px;");
  QHBoxLayout* ptcLayout = new QHBoxLayout(picTabControls);
  ptcLayout->setContentsMargins(12, 8, 12, 8);
  ptcLayout->setSpacing(8);
  
  QLabel* picTabLabel = new QLabel("📸 All Pictures", picTabControls);
  picTabLabel->setStyleSheet("color: #cdd6f4; font-size: 14px; font-weight: bold;");
  ptcLayout->addWidget(picTabLabel);
  
  QPushButton* picTabSaveBtn = new QPushButton("💾 Save All", picTabControls);
  picTabSaveBtn->setStyleSheet("background: #45475a; color: #cdd6f4; border: none; padding: 8px 16px; border-radius: 4px;");
  connect(picTabSaveBtn, &QPushButton::clicked, [this]() {
    if (m_currentPictureUrls.isEmpty()) { QMessageBox::information(this, "No Pictures", "No pictures available to save."); return; }
    saveAllPictures(m_currentPictureUrls, this);
  });
  ptcLayout->addWidget(picTabSaveBtn);
  
  m_picTabCountLabel = new QLabel("Total: 0", picTabControls);
  m_picTabCountLabel->setStyleSheet("color: #6c7086; font-size: 12px;");
  ptcLayout->addWidget(m_picTabCountLabel);
  ptcLayout->addStretch();
  picTabLayout->addWidget(picTabControls);
  
  QScrollArea* picTabScroll = new QScrollArea(this);
  picTabScroll->setStyleSheet(
    "QScrollArea { background: #181825; border: none; } "
    "QScrollBar:vertical { width: 8px; background: #1e1e2e; } "
    "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; min-height: 20px; } "
    "QScrollBar::handle:vertical:hover { background: #585b70; }");
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

  // --- All Videos Tab ---
  QWidget* videoListTabContent = new QWidget(this);
  videoListTabContent->setStyleSheet("background: #181825;");
  QVBoxLayout* videoListTabLayout = new QVBoxLayout(videoListTabContent);
  videoListTabLayout->setContentsMargins(0, 0, 0, 0);
  videoListTabLayout->setSpacing(0);

  QWidget* videoListTabControls = new QWidget(this);
  videoListTabControls->setStyleSheet("background: #313244; border-radius: 4px;");
  QHBoxLayout* vltcLayout = new QHBoxLayout(videoListTabControls);
  vltcLayout->setContentsMargins(12, 8, 12, 8);
  vltcLayout->setSpacing(8);

  QLabel* videoListTabLabel = new QLabel("🎬 All Videos", videoListTabControls);
  videoListTabLabel->setStyleSheet("color: #cba6f7; font-size: 14px; font-weight: bold;");
  vltcLayout->addWidget(videoListTabLabel);

  m_videoListTabCountLabel = new QLabel("Total: 0", videoListTabControls);
  m_videoListTabCountLabel->setStyleSheet("color: #6c7086; font-size: 12px;");
  vltcLayout->addWidget(m_videoListTabCountLabel);
  vltcLayout->addStretch();
  videoListTabLayout->addWidget(videoListTabControls);

  QScrollArea* videoListTabScroll = new QScrollArea(this);
  videoListTabScroll->setStyleSheet(
    "QScrollArea { background: #181825; border: none; } "
    "QScrollBar:vertical { width: 8px; background: #1e1e2e; } "
    "QScrollBar::handle:vertical { background: #45475a; border-radius: 4px; min-height: 20px; } "
    "QScrollBar::handle:vertical:hover { background: #585b70; }");
  videoListTabScroll->setWidgetResizable(true);

  QWidget* videoListTabContainer = new QWidget(this);
  videoListTabContainer->setStyleSheet("background: #181825;");
  QVBoxLayout* videoListLayout = new QVBoxLayout(videoListTabContainer);
  videoListLayout->setAlignment(Qt::AlignTop);
  videoListLayout->setContentsMargins(16, 16, 16, 16);
  videoListLayout->setSpacing(16);

  m_videoListTabScroll = videoListTabScroll;
  m_videoListTabContainer = videoListTabContainer;
  videoListTabScroll->setWidget(videoListTabContainer);
  videoListTabLayout->addWidget(videoListTabScroll);

  m_tabWidget->addTab(videoListTabContent, "🎬 Videos");

  // --- Monitor Tab ---
  QWidget* monitorTabContent = new QWidget(this);
  QVBoxLayout* monitorLayout = new QVBoxLayout(monitorTabContent);
  monitorLayout->setContentsMargins(16, 16, 16, 16);
  monitorLayout->setSpacing(10);

  QLabel* monitorTitle = new QLabel("Crawl Health Monitor", monitorTabContent);
  monitorTitle->setStyleSheet("font-size: 18px; font-weight: 700;");
  monitorLayout->addWidget(monitorTitle);

  auto createMonitorLabel = [monitorTabContent](const QString &text) {
    QLabel* label = new QLabel(text, monitorTabContent);
    label->setStyleSheet("background: #313244; border: 1px solid #45475a; border-radius: 8px; padding: 10px 12px; color: #cdd6f4;");
    return label;
  };

  m_monitorUsersLabel = createMonitorLabel("Users: processed=0 failed=0");
  monitorLayout->addWidget(m_monitorUsersLabel);
  m_monitorRequestsLabel = createMonitorLabel("Requests: total=0 failed=0");
  monitorLayout->addWidget(m_monitorRequestsLabel);
  m_monitorRetriesLabel = createMonitorLabel("Retries: 0");
  monitorLayout->addWidget(m_monitorRetriesLabel);
  m_monitor429Label = createMonitorLabel("HTTP 429: 0");
  monitorLayout->addWidget(m_monitor429Label);
  m_monitorQueueLabel = createMonitorLabel("Queue: pending=0 visited=0");
  monitorLayout->addWidget(m_monitorQueueLabel);
  m_monitorCurrentUidLabel = createMonitorLabel("Current UID: -");
  monitorLayout->addWidget(m_monitorCurrentUidLabel);
  monitorLayout->addStretch();

  m_tabWidget->addTab(monitorTabContent, "📈 Monitor");

  // --- Settings Tab ---
  QWidget* settingsTabContent = new QWidget(this);
  QVBoxLayout* settingsLayout = new QVBoxLayout(settingsTabContent);
  settingsLayout->setContentsMargins(16, 16, 16, 16);
  settingsLayout->setSpacing(12);

  QLabel* settingsTitle = new QLabel("Crawler Engine Settings", settingsTabContent);
  settingsTitle->setStyleSheet("font-size: 18px; font-weight: 700;");
  settingsLayout->addWidget(settingsTitle);

  QLabel* settingsHint = new QLabel(
      "These settings are saved to app_config.json and applied to new crawl tasks.",
      settingsTabContent);
  settingsHint->setStyleSheet("color: #6c7086;");
  settingsLayout->addWidget(settingsHint);

  QGroupBox* retryGroup = new QGroupBox("Retry Strategy", settingsTabContent);
  QFormLayout* retryForm = new QFormLayout(retryGroup);
  retryForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_requestProfileCombo = new QComboBox(retryGroup);
  m_requestProfileCombo->addItem("conservative");
  m_requestProfileCombo->addItem("balanced");
  m_requestProfileCombo->addItem("aggressive");
  m_requestProfileCombo->addItem("custom");
  const int profileIndex = m_requestProfileCombo->findText(
      QString::fromStdString(m_appConfig.request_profile),
      Qt::MatchFixedString);
  m_requestProfileCombo->setCurrentIndex(profileIndex >= 0 ? profileIndex : 1);

  QWidget* profileWidget = new QWidget(retryGroup);
  QHBoxLayout* profileLayout = new QHBoxLayout(profileWidget);
  profileLayout->setContentsMargins(0, 0, 0, 0);
  profileLayout->setSpacing(8);
  profileLayout->addWidget(m_requestProfileCombo);
  QPushButton* applyProfileBtn = new QPushButton("Apply", retryGroup);
  connect(applyProfileBtn, &QPushButton::clicked, [this]() {
    if (m_requestProfileCombo) {
      applyRequestProfile(m_requestProfileCombo->currentText());
    }
  });
  profileLayout->addWidget(applyProfileBtn);
  retryForm->addRow("Template", profileWidget);

  m_retryAttemptsSpin = new QSpinBox(retryGroup);
  m_retryAttemptsSpin->setRange(1, 20);
  m_retryAttemptsSpin->setValue(m_appConfig.retry_max_attempts);
  retryForm->addRow("Max Attempts", m_retryAttemptsSpin);

  m_retryBaseDelaySpin = new QSpinBox(retryGroup);
  m_retryBaseDelaySpin->setRange(0, 60000);
  m_retryBaseDelaySpin->setSingleStep(100);
  m_retryBaseDelaySpin->setSuffix(" ms");
  m_retryBaseDelaySpin->setValue(m_appConfig.retry_base_delay_ms);
  retryForm->addRow("Base Delay", m_retryBaseDelaySpin);

  m_retryMaxDelaySpin = new QSpinBox(retryGroup);
  m_retryMaxDelaySpin->setRange(0, 180000);
  m_retryMaxDelaySpin->setSingleStep(500);
  m_retryMaxDelaySpin->setSuffix(" ms");
  m_retryMaxDelaySpin->setValue(m_appConfig.retry_max_delay_ms);
  retryForm->addRow("Max Delay", m_retryMaxDelaySpin);

  m_retryBackoffSpin = new QDoubleSpinBox(retryGroup);
  m_retryBackoffSpin->setRange(1.0, 10.0);
  m_retryBackoffSpin->setDecimals(2);
  m_retryBackoffSpin->setSingleStep(0.1);
  m_retryBackoffSpin->setValue(m_appConfig.retry_backoff_factor);
  retryForm->addRow("Backoff Factor", m_retryBackoffSpin);
  settingsLayout->addWidget(retryGroup);

  QGroupBox* antiCrawlGroup = new QGroupBox("Anti-Crawl Stabilization", settingsTabContent);
  QFormLayout* antiCrawlForm = new QFormLayout(antiCrawlGroup);
  antiCrawlForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_requestMinIntervalSpin = new QSpinBox(antiCrawlGroup);
  m_requestMinIntervalSpin->setRange(0, 60000);
  m_requestMinIntervalSpin->setSingleStep(100);
  m_requestMinIntervalSpin->setSuffix(" ms");
  m_requestMinIntervalSpin->setValue(m_appConfig.request_min_interval_ms);
  antiCrawlForm->addRow("Min Interval", m_requestMinIntervalSpin);

  m_requestJitterSpin = new QSpinBox(antiCrawlGroup);
  m_requestJitterSpin->setRange(0, 10000);
  m_requestJitterSpin->setSingleStep(50);
  m_requestJitterSpin->setSuffix(" ms");
  m_requestJitterSpin->setValue(m_appConfig.request_jitter_ms);
  antiCrawlForm->addRow("Random Jitter", m_requestJitterSpin);

  m_cooldown429Spin = new QSpinBox(antiCrawlGroup);
  m_cooldown429Spin->setRange(0, 300000);
  m_cooldown429Spin->setSingleStep(500);
  m_cooldown429Spin->setSuffix(" ms");
  m_cooldown429Spin->setValue(m_appConfig.cooldown_429_ms);
  antiCrawlForm->addRow("429 Cooldown", m_cooldown429Spin);

  auto markProfileCustom = [this]() {
    if (!m_requestProfileCombo) {
      return;
    }
    if (m_requestProfileCombo->currentText() != "custom") {
      m_requestProfileCombo->setCurrentText("custom");
    }
  };
  connect(m_retryAttemptsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [markProfileCustom](int) { markProfileCustom(); });
  connect(m_retryBaseDelaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [markProfileCustom](int) { markProfileCustom(); });
  connect(m_retryMaxDelaySpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [markProfileCustom](int) { markProfileCustom(); });
  connect(m_retryBackoffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
          this, [markProfileCustom](double) { markProfileCustom(); });
  connect(m_requestMinIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [markProfileCustom](int) { markProfileCustom(); });
  connect(m_requestJitterSpin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [markProfileCustom](int) { markProfileCustom(); });
  connect(m_cooldown429Spin, QOverload<int>::of(&QSpinBox::valueChanged),
          this, [markProfileCustom](int) { markProfileCustom(); });

  settingsLayout->addWidget(antiCrawlGroup);

  QGroupBox* logGroup = new QGroupBox("Logging", settingsTabContent);
  QFormLayout* logForm = new QFormLayout(logGroup);
  logForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

  m_logLevelCombo = new QComboBox(logGroup);
  m_logLevelCombo->addItem("trace");
  m_logLevelCombo->addItem("debug");
  m_logLevelCombo->addItem("info");
  m_logLevelCombo->addItem("warn");
  m_logLevelCombo->addItem("error");
  m_logLevelCombo->addItem("critical");
  m_logLevelCombo->addItem("off");
  const int logLevelIndex = m_logLevelCombo->findText(
      QString::fromStdString(m_appConfig.log_level),
      Qt::MatchFixedString);
  m_logLevelCombo->setCurrentIndex(logLevelIndex >= 0 ? logLevelIndex : 2);
  logForm->addRow("Global Log Level", m_logLevelCombo);
  settingsLayout->addWidget(logGroup);

  QGroupBox* cookieGroup = new QGroupBox("Cookie Editor", settingsTabContent);
  QVBoxLayout* cookieLayout = new QVBoxLayout(cookieGroup);
  cookieLayout->setContentsMargins(8, 8, 8, 8);
  cookieLayout->setSpacing(8);

  m_cookiePathLabel = new QLabel(cookieGroup);
  m_cookiePathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_cookiePathLabel->setStyleSheet("color: #6c7086;");
  cookieLayout->addWidget(m_cookiePathLabel);

  m_cookieEditor = new QTextEdit(cookieGroup);
  m_cookieEditor->setAcceptRichText(false);
  m_cookieEditor->setMinimumHeight(180);
  m_cookieEditor->setPlaceholderText("Edit cookie.json content here (JSON object)");
  cookieLayout->addWidget(m_cookieEditor);

  QHBoxLayout* cookieBtnLayout = new QHBoxLayout();
  QPushButton* reloadCookieBtn = new QPushButton("Reload Cookie", cookieGroup);
  connect(reloadCookieBtn, &QPushButton::clicked, this, &MainWindow::loadCookieEditor);
  cookieBtnLayout->addWidget(reloadCookieBtn);

  QPushButton* saveCookieBtn = new QPushButton("Save Cookie", cookieGroup);
  connect(saveCookieBtn, &QPushButton::clicked, this, &MainWindow::saveCookieEditor);
  cookieBtnLayout->addWidget(saveCookieBtn);
  cookieBtnLayout->addStretch();
  cookieLayout->addLayout(cookieBtnLayout);

  settingsLayout->addWidget(cookieGroup);

  QHBoxLayout* settingsBtnLayout = new QHBoxLayout();
  settingsBtnLayout->addStretch();
  QPushButton* saveSettingsBtn = new QPushButton("Save Engine Settings", settingsTabContent);
  connect(saveSettingsBtn, &QPushButton::clicked, [this]() {
    syncSettingsUiToAppConfig();
    m_appConfig.save();
    setupLogSink();
    appendLog(QString("Engine settings saved. Log level=%1, retry=%2, interval=%3ms, jitter=%4ms")
                  .arg(QString::fromStdString(m_appConfig.log_level))
                  .arg(m_appConfig.retry_max_attempts)
                  .arg(m_appConfig.request_min_interval_ms)
                  .arg(m_appConfig.request_jitter_ms));
  });
  settingsBtnLayout->addWidget(saveSettingsBtn);
  settingsLayout->addLayout(settingsBtnLayout);
  settingsLayout->addStretch();

  loadCookieEditor();

  m_tabWidget->addTab(settingsTabContent, "⚙ Settings");

  setupDownloadManagerTab();

  // --- Log Tab ---
  m_tabWidget->addTab(m_logPanel, "📜 Logs");

  mainLayout->addWidget(m_tabWidget, 1);
  setCentralWidget(centralWidget);

  connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
  connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);

  m_themeCombo->setCurrentIndex(m_currentTheme);
  applyTheme(m_currentTheme);
}
