#include "mainwindow.hpp"
#include <QPainterPath>
#include <QPen>
#include <QRandomGenerator>
#include <QtMath>

QPointF MainWindow::getRandomPosition() {
  double angle = QRandomGenerator::global()->bounded(360.0);
  double radius = 100.0 + m_nodeCount * 30.0;
  double x = radius * qCos(angle * M_PI / 180.0);
  double y = radius * qSin(angle * M_PI / 180.0);
  return QPointF(x, y);
}

void MainWindow::addUserNode(uint64_t uid, const QString& name, const QList<uint64_t>& followers, const QList<uint64_t>& fans) {
  if (m_nodes.contains(uid)) return;

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
    if (!m_nodes.contains(uid)) continue;
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
