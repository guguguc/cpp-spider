#ifndef LOG_PANEL_HPP
#define LOG_PANEL_HPP

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QColor>
#include <QFileDialog>
#include <QScrollBar>
#include <vector>
#include <mutex>

enum class LogLevel {
  Trace = 0,
  Debug,
  Info,
  Warn,
  Error,
  Critical,
  App  // application-level messages (from MainWindow)
};

struct LogEntry {
  QDateTime timestamp;
  LogLevel level;
  QString message;
  QString source;  // "spider", "app", etc.
};

class LogPanel : public QWidget {
  Q_OBJECT

public:
  explicit LogPanel(QWidget* parent = nullptr);

  void appendLog(LogLevel level, const QString& message, const QString& source = "app");
  void clear();
  void setMaxEntries(int max);
  int entryCount() const;

  // Theme support
  void applyThemeColors(const QColor& bg, const QColor& panel_bg,
                        const QColor& card_bg, const QColor& border,
                        const QColor& text, const QColor& text_muted,
                        const QColor& accent, bool dark_mode);

public slots:
  void onFilterChanged();
  void onSearchChanged(const QString& text);
  void onExportClicked();
  void onClearClicked();
  void onAutoScrollToggled(bool checked);

private:
  void setupUi();
  void refreshDisplay();
  QString levelToString(LogLevel level) const;
  QColor levelToColor(LogLevel level) const;
  QString formatEntry(const LogEntry& entry) const;

  QTextEdit* m_logDisplay;
  QLineEdit* m_searchInput;
  QComboBox* m_levelFilter;
  QComboBox* m_sourceFilter;
  QPushButton* m_clearBtn;
  QPushButton* m_exportBtn;
  QCheckBox* m_autoScrollCheck;
  QLabel* m_countLabel;

  std::vector<LogEntry> m_entries;
  std::mutex m_entriesMutex;
  int m_maxEntries;
  bool m_autoScroll;
  bool m_darkMode;

  // Theme colors
  QColor m_bgColor;
  QColor m_textColor;
  QColor m_mutedColor;
};

#endif  // LOG_PANEL_HPP
