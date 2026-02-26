#include "log_panel.hpp"
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <fmt/core.h>

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
    , m_logDisplay(new QTextEdit(this))
    , m_searchInput(new QLineEdit(this))
    , m_levelFilter(new QComboBox(this))
    , m_sourceFilter(new QComboBox(this))
    , m_clearBtn(new QPushButton("Clear", this))
    , m_exportBtn(new QPushButton("Export", this))
    , m_autoScrollCheck(new QCheckBox("Auto Scroll", this))
    , m_countLabel(new QLabel("0 entries", this))
    , m_maxEntries(5000)
    , m_autoScroll(true)
    , m_darkMode(true)
    , m_bgColor("#181825")
    , m_textColor("#cdd6f4")
    , m_mutedColor("#6c7086") {
  setupUi();
}

void LogPanel::setupUi() {
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Toolbar
  QWidget* toolbar = new QWidget(this);
  QHBoxLayout* tbLayout = new QHBoxLayout(toolbar);
  tbLayout->setContentsMargins(8, 6, 8, 6);
  tbLayout->setSpacing(6);

  QLabel* filterLabel = new QLabel("Level:", this);
  tbLayout->addWidget(filterLabel);

  m_levelFilter->addItem("All");
  m_levelFilter->addItem("Trace");
  m_levelFilter->addItem("Debug");
  m_levelFilter->addItem("Info");
  m_levelFilter->addItem("Warn");
  m_levelFilter->addItem("Error");
  m_levelFilter->addItem("Critical");
  m_levelFilter->addItem("App");
  m_levelFilter->setCurrentIndex(0);
  m_levelFilter->setFixedWidth(90);
  tbLayout->addWidget(m_levelFilter);

  QLabel* sourceLabel = new QLabel("Source:", this);
  tbLayout->addWidget(sourceLabel);

  m_sourceFilter->addItem("All");
  m_sourceFilter->addItem("spider");
  m_sourceFilter->addItem("app");
  m_sourceFilter->setCurrentIndex(0);
  m_sourceFilter->setFixedWidth(90);
  tbLayout->addWidget(m_sourceFilter);

  m_searchInput->setPlaceholderText("Search logs...");
  m_searchInput->setFixedWidth(200);
  m_searchInput->setClearButtonEnabled(true);
  tbLayout->addWidget(m_searchInput);

  tbLayout->addStretch();

  m_autoScrollCheck->setChecked(true);
  tbLayout->addWidget(m_autoScrollCheck);
  tbLayout->addWidget(m_countLabel);
  tbLayout->addWidget(m_clearBtn);
  tbLayout->addWidget(m_exportBtn);

  mainLayout->addWidget(toolbar);

  // Log display
  m_logDisplay->setReadOnly(true);
  m_logDisplay->setLineWrapMode(QTextEdit::NoWrap);
  m_logDisplay->setFont(QFont("Monospace", 10));
  mainLayout->addWidget(m_logDisplay, 1);

  // Connections
  connect(m_levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &LogPanel::onFilterChanged);
  connect(m_sourceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &LogPanel::onFilterChanged);
  connect(m_searchInput, &QLineEdit::textChanged,
          this, &LogPanel::onSearchChanged);
  connect(m_clearBtn, &QPushButton::clicked,
          this, &LogPanel::onClearClicked);
  connect(m_exportBtn, &QPushButton::clicked,
          this, &LogPanel::onExportClicked);
  connect(m_autoScrollCheck, &QCheckBox::toggled,
          this, &LogPanel::onAutoScrollToggled);
}

QString LogPanel::levelToString(LogLevel level) const {
  switch (level) {
    case LogLevel::Trace:    return "TRACE";
    case LogLevel::Debug:    return "DEBUG";
    case LogLevel::Info:     return "INFO";
    case LogLevel::Warn:     return "WARN";
    case LogLevel::Error:    return "ERROR";
    case LogLevel::Critical: return "CRIT";
    case LogLevel::App:      return "APP";
  }
  return "UNKNOWN";
}

QColor LogPanel::levelToColor(LogLevel level) const {
  switch (level) {
    case LogLevel::Trace:    return m_darkMode ? QColor("#7f849c") : QColor("#8c8fa1");
    case LogLevel::Debug:    return m_darkMode ? QColor("#89b4fa") : QColor("#1e66f5");
    case LogLevel::Info:     return m_darkMode ? QColor("#a6e3a1") : QColor("#40a02b");
    case LogLevel::Warn:     return m_darkMode ? QColor("#f9e2af") : QColor("#df8e1d");
    case LogLevel::Error:    return m_darkMode ? QColor("#f38ba8") : QColor("#d20f39");
    case LogLevel::Critical: return m_darkMode ? QColor("#f38ba8") : QColor("#d20f39");
    case LogLevel::App:      return m_darkMode ? QColor("#cba6f7") : QColor("#8839ef");
  }
  return m_textColor;
}

QString LogPanel::formatEntry(const LogEntry& entry) const {
  QString time = entry.timestamp.toString("hh:mm:ss.zzz");
  QString lvl = levelToString(entry.level);
  QColor color = levelToColor(entry.level);
  return QString("<span style='color:%1'>[%2]</span> "
                 "<span style='color:%3;font-weight:bold'>[%4]</span> "
                 "<span style='color:%5'>[%6]</span> "
                 "<span style='color:%7'>%8</span>")
      .arg(m_mutedColor.name(), time,
           color.name(), lvl,
           m_mutedColor.name(), entry.source,
            m_textColor.name(), entry.message.toHtmlEscaped());
}

void LogPanel::appendLog(LogLevel level, const QString& message, const QString& source) {
  LogEntry entry;
  entry.timestamp = QDateTime::currentDateTime();
  entry.level = level;
  entry.message = message;
  entry.source = source;

  {
    std::lock_guard<std::mutex> lock(m_entriesMutex);
    m_entries.push_back(entry);
    if (static_cast<int>(m_entries.size()) > m_maxEntries) {
      m_entries.erase(m_entries.begin(),
                      m_entries.begin() + static_cast<int>(m_entries.size()) - m_maxEntries);
    }
  }

  // Check if entry passes current filters
  int levelIdx = m_levelFilter->currentIndex();
  QString sourceFilter = m_sourceFilter->currentText();
  QString searchText = m_searchInput->text();

  bool passLevel = (levelIdx == 0) ||
      (levelIdx - 1 == static_cast<int>(level));
  bool passSource = (sourceFilter == "All") || (source == sourceFilter);
  bool passSearch = searchText.isEmpty() ||
      message.contains(searchText, Qt::CaseInsensitive);

  if (passLevel && passSource && passSearch) {
    m_logDisplay->append(formatEntry(entry));
    if (m_autoScroll) {
      QScrollBar* sb = m_logDisplay->verticalScrollBar();
      sb->setValue(sb->maximum());
    }
  }

  m_countLabel->setText(QString("%1 entries").arg(m_entries.size()));
}

void LogPanel::clear() {
  std::lock_guard<std::mutex> lock(m_entriesMutex);
  m_entries.clear();
  m_logDisplay->clear();
  m_countLabel->setText("0 entries");
}

void LogPanel::setMaxEntries(int max) {
  m_maxEntries = max;
}

int LogPanel::entryCount() const {
  return static_cast<int>(m_entries.size());
}

void LogPanel::refreshDisplay() {
  m_logDisplay->clear();
  std::lock_guard<std::mutex> lock(m_entriesMutex);

  int levelIdx = m_levelFilter->currentIndex();
  QString sourceFilter = m_sourceFilter->currentText();
  QString searchText = m_searchInput->text();

  for (const auto& entry : m_entries) {
    bool passLevel = (levelIdx == 0) ||
        (levelIdx - 1 == static_cast<int>(entry.level));
    bool passSource = (sourceFilter == "All") ||
        (entry.source == sourceFilter);
    bool passSearch = searchText.isEmpty() ||
        entry.message.contains(searchText, Qt::CaseInsensitive);

    if (passLevel && passSource && passSearch) {
      m_logDisplay->append(formatEntry(entry));
    }
  }

  if (m_autoScroll) {
    QScrollBar* sb = m_logDisplay->verticalScrollBar();
    sb->setValue(sb->maximum());
  }
}

void LogPanel::onFilterChanged() {
  refreshDisplay();
}

void LogPanel::onSearchChanged(const QString& text) {
  Q_UNUSED(text);
  refreshDisplay();
}

void LogPanel::onClearClicked() {
  clear();
}

void LogPanel::onExportClicked() {
  QString filePath = QFileDialog::getSaveFileName(
      this, "Export Logs", "spider_logs.txt",
      "Text Files (*.txt);;CSV Files (*.csv);;All Files (*)");
  if (filePath.isEmpty()) return;

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "Export Failed",
                         QString("Cannot open file: %1").arg(filePath));
    return;
  }

  QTextStream out(&file);
  std::lock_guard<std::mutex> lock(m_entriesMutex);

  bool csv = filePath.endsWith(".csv", Qt::CaseInsensitive);
  if (csv) {
    out << "Timestamp,Level,Source,Message\n";
  }

  for (const auto& entry : m_entries) {
    QString time = entry.timestamp.toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString lvl = levelToString(entry.level);
    if (csv) {
      QString escaped = entry.message;
      escaped.replace("\"", "\"\"");
      out << QString("\"%1\",\"%2\",\"%3\",\"%4\"\n")
              .arg(time, lvl, entry.source, escaped);
    } else {
      out << QString("[%1] [%2] [%3] %4\n")
              .arg(time, lvl, entry.source, entry.message);
    }
  }

  file.close();
}

void LogPanel::onAutoScrollToggled(bool checked) {
  m_autoScroll = checked;
  if (m_autoScroll) {
    QScrollBar* sb = m_logDisplay->verticalScrollBar();
    sb->setValue(sb->maximum());
  }
}

void LogPanel::applyThemeColors(const QColor& bg, const QColor& panel_bg,
                                const QColor& card_bg, const QColor& border,
                                const QColor& text, const QColor& text_muted,
                                const QColor& accent, bool dark_mode) {
  m_darkMode = dark_mode;
  m_bgColor = bg;
  m_textColor = text;
  m_mutedColor = text_muted;

  m_logDisplay->setStyleSheet(
      QString("QTextEdit { background: %1; border: 1px solid %2; "
              "border-radius: 4px; color: %3; font-family: monospace; }")
          .arg(card_bg.name(), border.name(), text.name()));

  refreshDisplay();
}
