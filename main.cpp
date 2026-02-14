#include "mainwindow.hpp"
#include <QApplication>
#include <mongocxx/instance.hpp>

int main(int argc, char* argv[]) {
  mongocxx::instance inst{};
  QApplication app(argc, argv);
  MainWindow window;
  window.show();
  return app.exec();
}
