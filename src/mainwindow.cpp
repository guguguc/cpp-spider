#include "mainwindow.hpp"
#include "spider.hpp"
#include <httplib.h>
#include <QMediaPlayer>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_appConfig(AppConfig::load())
    , m_startBtn(new QPushButton("▶ Start", this))
    , m_stopBtn(new QPushButton("■ Stop", this))
    , m_logPanel(new LogPanel(this))
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
    , m_targetUid(m_appConfig.default_uid)
   , m_nodeCount(0)
   , m_currentTheme(0)
   , m_activeDownloads(0)
   , m_videoWidget(nullptr)
   , m_weiboScroll(nullptr)
   , m_weiboContainer(nullptr) {
  m_imageClient = std::make_unique<httplib::Client>(m_appConfig.image_host);
  m_imageClient->set_follow_location(true);
  m_imageClient->set_decompress(true);
  m_imageClient->set_keep_alive(true);
  m_imageClient->set_connection_timeout(30, 0);
  m_imageClient->set_read_timeout(30, 0);
  m_videoPlayer = std::make_unique<QMediaPlayer>();
  initThemes();
  loadConfig();
  setupUi();
  setupLogSink();
}

MainWindow::~MainWindow() {}

void MainWindow::initThemes() {
  m_themes = {
    {"Ocean", QColor("#00D4FF"), QColor("#0077B6"), QColor("#00F5D4"), QColor("#FF6B6B"), QColor("#E8F4F8"), QColor("#023E8A")},
    {"Midnight", QColor("#7C3AED"), QColor("#4C1D95"), QColor("#A78BFA"), QColor("#F472B6"), QColor("#0F172A"), QColor("#CBD5E1")},
    {"Aurora", QColor("#10B981"), QColor("#047857"), QColor("#6EE7B7"), QColor("#F43F5E"), QColor("#ECFDF5"), QColor("#064E3B")},
    {"Coral", QColor("#F97316"), QColor("#C2410C"), QColor("#FDBA74"), QColor("#FB7185"), QColor("#FFF7ED"), QColor("#7C2D12")},
    {"Neon", QColor("#E879F9"), QColor("#A21CAF"), QColor("#D8B4FE"), QColor("#22D3EE"), QColor("#1E1B4B"), QColor("#E9D5FF")},
    {"Slate", QColor("#3B82F6"), QColor("#1D4ED8"), QColor("#60A5FA"), QColor("#F87171"), QColor("#0F172A"), QColor("#F1F5F9")},
    {"Sunset", QColor("#F59E0B"), QColor("#B45309"), QColor("#FB923C"), QColor("#EF4444"), QColor("#FFF7ED"), QColor("#7C2D12")},
    {"Forest", QColor("#22C55E"), QColor("#166534"), QColor("#86EFAC"), QColor("#F97316"), QColor("#F0FDF4"), QColor("#14532D")},
    {"Skyline", QColor("#38BDF8"), QColor("#0369A1"), QColor("#93C5FD"), QColor("#F43F5E"), QColor("#F0F9FF"), QColor("#0C4A6E")},
    {"Rose Gold", QColor("#FB7185"), QColor("#BE123C"), QColor("#F9A8D4"), QColor("#F59E0B"), QColor("#FFF1F2"), QColor("#881337")}
  };
}
