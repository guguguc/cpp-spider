#include "mainwindow.hpp"

#include <QtTest/QtTest>

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

namespace {

QComboBox* find_request_profile_combo(MainWindow* window) {
  const auto combos = window->findChildren<QComboBox*>();
  for (QComboBox* combo : combos) {
    if (combo->findText("conservative") >= 0 &&
        combo->findText("balanced") >= 0 &&
        combo->findText("aggressive") >= 0 &&
        combo->findText("custom") >= 0) {
      return combo;
    }
  }
  return nullptr;
}

QSpinBox* find_retry_attempts_spin(MainWindow* window) {
  const auto spins = window->findChildren<QSpinBox*>();
  for (QSpinBox* spin : spins) {
    if (spin->minimum() == 1 && spin->maximum() == 20) {
      return spin;
    }
  }
  return nullptr;
}

QPushButton* find_apply_profile_button(MainWindow* window) {
  const auto buttons = window->findChildren<QPushButton*>();
  for (QPushButton* button : buttons) {
    if (button->text() == "Apply") {
      return button;
    }
  }
  return nullptr;
}

}

class MainWindowUiTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() {
    qRegisterMetaType<QList<uint64_t>>("QList<uint64_t>");
  }

  void has_expected_tabs_and_controls() {
    MainWindow window;

    auto* tabs = window.findChild<QTabWidget*>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->count(), 9);

    QStringList tab_texts;
    for (int i = 0; i < tabs->count(); ++i) {
      tab_texts.push_back(tabs->tabText(i));
    }

    QVERIFY(tab_texts.join("|").contains("Graph"));
    QVERIFY(tab_texts.join("|").contains("Weibo"));
    QVERIFY(tab_texts.join("|").contains("Monitor"));
    QVERIFY(tab_texts.join("|").contains("Settings"));
    QVERIFY(tab_texts.join("|").contains("Downloads"));

    bool has_start = false;
    bool has_stop = false;
    bool stop_disabled = false;
    const auto buttons = window.findChildren<QPushButton*>();
    for (QPushButton* button : buttons) {
      if (button->text().contains("Start")) {
        has_start = true;
      }
      if (button->text().contains("Stop")) {
        has_stop = true;
        if (!button->isEnabled()) {
          stop_disabled = true;
        }
      }
    }

    QVERIFY(has_start);
    QVERIFY(has_stop);
    QVERIFY(stop_disabled);
  }

  void request_profile_apply_updates_spinboxes() {
    MainWindow window;

    QComboBox* profile_combo = find_request_profile_combo(&window);
    QVERIFY(profile_combo != nullptr);
    QPushButton* apply_btn = find_apply_profile_button(&window);
    QVERIFY(apply_btn != nullptr);

    profile_combo->setCurrentText("aggressive");
    QTest::mouseClick(apply_btn, Qt::LeftButton);

    const auto spins = window.findChildren<QSpinBox*>();
    bool has_attempts = false;
    bool has_base_delay = false;
    bool has_max_delay = false;
    bool has_min_interval = false;
    bool has_jitter = false;
    bool has_cooldown = false;

    for (QSpinBox* spin : spins) {
      if (spin->value() == 8) has_attempts = true;
      if (spin->value() == 500) has_base_delay = true;
      if (spin->value() == 8000) has_max_delay = true;
      if (spin->value() == 300) has_min_interval = true;
      if (spin->value() == 200) has_jitter = true;
      if (spin->value() == 10000) has_cooldown = true;
    }

    QVERIFY(has_attempts);
    QVERIFY(has_base_delay);
    QVERIFY(has_max_delay);
    QVERIFY(has_min_interval);
    QVERIFY(has_jitter);
    QVERIFY(has_cooldown);
  }

  void editing_retry_value_marks_profile_custom() {
    MainWindow window;

    QComboBox* profile_combo = find_request_profile_combo(&window);
    QVERIFY(profile_combo != nullptr);

    profile_combo->setCurrentText("balanced");
    QSpinBox* attempts_spin = find_retry_attempts_spin(&window);
    QVERIFY(attempts_spin != nullptr);

    attempts_spin->setValue(7);
    QCOMPARE(profile_combo->currentText(), QString("custom"));
  }

  void metrics_slot_updates_monitor_labels() {
    MainWindow window;

    const bool invoked = QMetaObject::invokeMethod(
        &window,
        "onMetricsUpdated",
        Qt::DirectConnection,
        Q_ARG(qulonglong, static_cast<qulonglong>(12)),
        Q_ARG(qulonglong, static_cast<qulonglong>(2)),
        Q_ARG(qulonglong, static_cast<qulonglong>(40)),
        Q_ARG(qulonglong, static_cast<qulonglong>(3)),
        Q_ARG(qulonglong, static_cast<qulonglong>(6)),
        Q_ARG(qulonglong, static_cast<qulonglong>(1)),
        Q_ARG(qulonglong, static_cast<qulonglong>(9)),
        Q_ARG(qulonglong, static_cast<qulonglong>(15)),
        Q_ARG(qulonglong, static_cast<qulonglong>(123456)));
    QVERIFY(invoked);

    bool saw_users = false;
    bool saw_requests = false;
    bool saw_retries = false;
    bool saw_http429 = false;
    bool saw_queue = false;
    bool saw_uid = false;

    const auto labels = window.findChildren<QLabel*>();
    for (QLabel* label : labels) {
      const QString text = label->text();
      if (text == "Users: processed=12 failed=2") saw_users = true;
      if (text == "Requests: total=40 failed=3") saw_requests = true;
      if (text == "Retries: 6") saw_retries = true;
      if (text == "HTTP 429: 1") saw_http429 = true;
      if (text == "Queue: pending=9 visited=15") saw_queue = true;
      if (text == "Current UID: 123456") saw_uid = true;
    }

    QVERIFY(saw_users);
    QVERIFY(saw_requests);
    QVERIFY(saw_retries);
    QVERIFY(saw_http429);
    QVERIFY(saw_queue);
    QVERIFY(saw_uid);
  }
};

QTEST_MAIN(MainWindowUiTest)
#include "ui_mainwindow_test.moc"
