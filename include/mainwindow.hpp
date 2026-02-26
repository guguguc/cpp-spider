#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include "graph_layout.hpp"
#include "log_panel.hpp"
#include <httplib.h>

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QString>
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QMap>
#include <QCheckBox>
#include <QComboBox>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QGraphicsSceneMouseEvent>
#include <QColor>
#include <QToolBar>
#include <QLineEdit>
#include <QTabWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QPixmap>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <memory>
#include <tuple>
#include <functional>
#include <atomic>
#include <mutex>

class Spider;
class User;

struct Theme {
  QString name;
  QColor nodeBrush;
  QColor nodePen;
  QColor followerLine;
  QColor fanLine;
  QColor background;
  QColor text;
};

class ZoomGraphicsView : public QGraphicsView {
public:
  explicit ZoomGraphicsView(QWidget* parent = nullptr) : QGraphicsView(parent) {
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setFrameShape(QFrame::NoFrame);
  }

protected:
  void wheelEvent(QWheelEvent* event) override {
    double scaleFactor = 1.15;
    if (event->angleDelta().y() > 0) {
      scale(scaleFactor, scaleFactor);
    } else {
      scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    }
    event->accept();
  }

  void keyPressEvent(QKeyEvent* event) override {
    double scaleFactor = 1.15;
    if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal) {
      scale(scaleFactor, scaleFactor);
    } else if (event->key() == Qt::Key_Minus) {
      scale(1.0 / scaleFactor, 1.0 / scaleFactor);
    } else if (event->key() == Qt::Key_0) {
      resetTransform();
    } else {
      QGraphicsView::keyPressEvent(event);
    }
    event->accept();
  }
};

class NodeItem : public QGraphicsEllipseItem {
public:
  explicit NodeItem(qreal x, qreal y, qreal width, qreal height, QGraphicsItem* parent = nullptr)
      : QGraphicsEllipseItem(x, y, width, height), m_dragging(false), m_theme(nullptr), m_uid(0), m_label(nullptr), m_clickStartPos(0, 0), m_clickCallback(nullptr) {
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setBrush(QBrush(Qt::cyan));
    setPen(QPen(Qt::darkBlue, 2));
    setAcceptHoverEvents(true);
  }

  void setUid(uint64_t uid) { m_uid = uid; }
  uint64_t uid() const { return m_uid; }
  void setLabel(QGraphicsTextItem* label) { m_label = label; }
  void setClickCallback(std::function<void(uint64_t)> callback) { m_clickCallback = callback; }

  void setTheme(const Theme& theme) {
    m_theme = &theme;
    setBrush(QBrush(theme.nodeBrush));
    setPen(QPen(theme.nodePen, 2));
  }
  void addLine(QGraphicsPathItem* line, NodeItem* other) { 
    m_lines.append(qMakePair(line, other)); 
  }
  void updateLines();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
    m_dragging = true;
    m_clickStartPos = event->screenPos();
    QGraphicsEllipseItem::mousePressEvent(event);
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
    m_dragging = false;
    QPointF clickEndPos = event->screenPos();
    if ((clickEndPos - m_clickStartPos).manhattanLength() < 10 && m_clickCallback) {
      m_clickCallback(m_uid);
    }
    updateLines();
    QGraphicsEllipseItem::mouseReleaseEvent(event);
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override {
    if (m_dragging) {
      if (m_label) {
        m_label->setPos(pos().x() - 30, pos().y() + 25);
      }
      updateLines();
    }
    QGraphicsEllipseItem::mouseMoveEvent(event);
  }

 private:
  bool m_dragging;
 public:
  QList<QPair<QGraphicsPathItem*, NodeItem*>> m_lines;
 private:
  const Theme* m_theme;
  uint64_t m_uid;
  QGraphicsTextItem* m_label;
  QPointF m_clickStartPos;
  std::function<void(uint64_t)> m_clickCallback;
};

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow();

private slots:
   void onStartClicked();
   void onStopClicked();
   void appendLog(const QString& message);
   void appendSpiderLog(int level, const QString& message);
   void onUserFetched(uint64_t uid, const QString& name, const QList<uint64_t>& followers, const QList<uint64_t>& fans);
   void onWeiboFetched(uint64_t uid, const QString& weibo);
   void onLayoutChanged(int index);
   void onApplyLayoutClicked();
   void showNodeWeibo(uint64_t uid);
    void updateWeiboStats(int totalWeibo, int totalVideo);
    void showAllPictures(uint64_t uid);
    void showAllVideos(uint64_t uid);
    std::string loadCookies();

private:
   void setupUi();
   void setupLogSink();
   void runSpider();
   void loadConfig();
   void addUserNode(uint64_t uid, const QString& name, const QList<uint64_t>& followers, const QList<uint64_t>& fans);
   QPointF getRandomPosition();
   void saveConfig();
   void applyTheme(int index);
   void initThemes();
   void applyLayout(LayoutType type);
   void updateNodePositions(const QMap<uint64_t, QPointF>& newPositions);
   void downloadVideo(const QString& videoUrl, QWidget* parent);
   void saveAllPictures(const QList<QString>& picUrls, QWidget* parent);
   void loadImageAsync(const QString& picUrl, QLabel* picLabel, int maxSize);

  QPushButton* m_startBtn;
  QPushButton* m_stopBtn;
  LogPanel* m_logPanel;
  ZoomGraphicsView* m_graphView;
  QGraphicsScene* m_graphScene;
  QCheckBox* m_crawlWeiboCheck;
  QCheckBox* m_crawlFansCheck;
  QCheckBox* m_crawlFollowersCheck;
  QCheckBox* m_playVideoCheck;
  QLineEdit* m_uidInput;
  QComboBox* m_themeCombo;
  QComboBox* m_layoutCombo;
  QPushButton* m_applyLayoutBtn;
  QTabWidget* m_tabWidget;
  std::unique_ptr<Spider> m_spider;
  bool m_running;
  bool m_crawlWeibo;
  uint64_t m_targetUid;

  QList<Theme> m_themes;
  int m_currentTheme;

  QMap<uint64_t, NodeItem*> m_nodes;
  QMap<uint64_t, QPointF> m_positions;
  QMap<uint64_t, QGraphicsPathItem*> m_lines;
  QMap<uint64_t, bool> m_lineIsFollower;
  QMap<uint64_t, std::vector<uint64_t>> m_adjacency;
  QMap<uint64_t, QGraphicsTextItem*> m_labels;
  
  struct WeiboData {
    QString timestamp;
    QString text;
    std::vector<std::string> pics;
    std::string video_url;
  };
  QMap<uint64_t, std::vector<WeiboData>> m_weibos;
   QMap<QString, QPixmap> m_imageCache;
   std::unique_ptr<httplib::Client> m_imageClient;
   std::mutex m_imageClientMutex;
   std::atomic<int> m_activeDownloads;
   static const int MAX_CONCURRENT_DOWNLOADS = 8;
   int m_nodeCount;
  LayoutType m_currentLayout;
  
  std::unique_ptr<QMediaPlayer> m_videoPlayer;
  QVideoWidget* m_videoWidget;
  QWidget* m_videoPlayerWidget;
  QScrollArea* m_weiboScroll;
  QWidget* m_weiboContainer;
  QLabel* m_totalWeiboLabel;
  QLabel* m_totalVideoLabel;
  
   QVideoWidget* m_videoTabWidget;
   std::unique_ptr<QMediaPlayer> m_videoTabPlayer;
   std::unique_ptr<QAudioOutput> m_videoTabAudio;
   QLabel* m_videoTabStatus;
   QString m_currentVideoUrl;
   
     QScrollArea* m_picTabScroll;
    QWidget* m_picTabContainer;
    QLabel* m_picTabCountLabel;
    QList<QString> m_currentPictureUrls;
    uint64_t m_currentPictureUid;

    QScrollArea* m_videoListTabScroll;
    QWidget* m_videoListTabContainer;
    QLabel* m_videoListTabCountLabel;
    QList<QString> m_currentVideoUrls;
    uint64_t m_currentVideoListUid;
};

#endif  // MAINWINDOW_HPP
