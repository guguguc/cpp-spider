#include "mainwindow.hpp"
#include <algorithm>
#include <QMenu>
#include <QMessageBox>
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
  if (!followers.isEmpty() || !m_followersByUid.contains(uid)) {
    m_followersByUid[uid] = followers;
  }
  if (!fans.isEmpty() || !m_fansByUid.contains(uid)) {
    m_fansByUid[uid] = fans;
  }

  const Theme& theme = m_themes[m_currentTheme];

  auto ensure_node = [this, &theme](uint64_t node_uid, const QString &node_name) {
    NodeItem* node = nullptr;
    if (!m_nodes.contains(node_uid)) {
      QPointF pos = getRandomPosition();
      m_positions[node_uid] = pos;
      m_nodeCount++;

      node = new NodeItem(-20, -20, 40, 40);
      node->setPos(pos);
      node->setUid(node_uid);
      node->setClickCallback([this](uint64_t clickedUid) {
        QMetaObject::invokeMethod(this, "showNodeWeibo", Qt::QueuedConnection,
                                  Q_ARG(uint64_t, clickedUid));
      });
      node->setContextMenuCallback([this](uint64_t clickedUid, const QPoint& screenPos) {
        showNodeContextMenu(clickedUid, screenPos);
      });
      m_graphScene->addItem(node);
      m_nodes[node_uid] = node;

      QGraphicsTextItem* label = new QGraphicsTextItem(
          node_name.isEmpty() ? QString::number(node_uid) : node_name);
      label->setDefaultTextColor(theme.text);
      label->setPos(pos.x() - 30, pos.y() + 25);
      m_graphScene->addItem(label);
      m_labels[node_uid] = label;
      node->setLabel(label);
      node->setTheme(theme);
    } else {
      node = m_nodes[node_uid];
      node->setTheme(theme);
      if (!node_name.isEmpty() && m_labels.contains(node_uid)) {
        const QString current = m_labels[node_uid]->toPlainText();
        if (current == QString::number(node_uid)) {
          m_labels[node_uid]->setPlainText(node_name);
        }
      }
    }
    return node;
  };

  NodeItem* node = ensure_node(uid, name);
  QPointF pos = node->pos();

  auto add_undirected_adj = [this](uint64_t a, uint64_t b) {
    auto &av = m_adjacency[a];
    if (std::find(av.begin(), av.end(), b) == av.end()) {
      av.push_back(b);
    }
    auto &bv = m_adjacency[b];
    if (std::find(bv.begin(), bv.end(), a) == bv.end()) {
      bv.push_back(a);
    }
  };

  for (uint64_t follower_id : followers) {
    if (follower_id == uid) {
      continue;
    }
    add_undirected_adj(uid, follower_id);
    NodeItem* otherNode = ensure_node(follower_id, QString());
    uint64_t lineKey = uid * 1000000 + follower_id;
    if (!m_lines.contains(lineKey)) {
      QPointF endPos = otherNode->pos();
      QPainterPath path;
      path.moveTo(pos);
      QPointF ctrl1 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, pos.y() - 30);
      QPointF ctrl2 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, endPos.y() - 30);
      path.cubicTo(ctrl1, ctrl2, endPos);
      QGraphicsPathItem* line = new QGraphicsPathItem(path);
      line->setPen(QPen(theme.followerLine, 1.5));
      m_graphScene->addItem(line);
      m_lines[lineKey] = line;
      m_lineIsFollower[lineKey] = true;
      node->addLine(line, otherNode);
      otherNode->addLine(line, node);
    }
  }

  for (uint64_t fan_id : fans) {
    if (fan_id == uid) {
      continue;
    }
    add_undirected_adj(uid, fan_id);
    NodeItem* otherNode = ensure_node(fan_id, QString());
    uint64_t lineKey = uid * 1000000 + fan_id;
    if (!m_lines.contains(lineKey)) {
      QPointF endPos = otherNode->pos();
      QPainterPath path;
      path.moveTo(pos);
      QPointF ctrl1 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, pos.y() + 30);
      QPointF ctrl2 = QPointF(pos.x() + (endPos.x() - pos.x()) * 0.5, endPos.y() + 30);
      path.cubicTo(ctrl1, ctrl2, endPos);
      QGraphicsPathItem* line = new QGraphicsPathItem(path);
      line->setPen(QPen(theme.fanLine, 1.5));
      m_graphScene->addItem(line);
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

void MainWindow::showNodeContextMenu(uint64_t uid, const QPoint& screenPos) {
  QMenu menu(this);
  QAction* viewFollowers = menu.addAction("View Followers");
  QAction* viewFans = menu.addAction("View Fans");
  QAction* selected = menu.exec(screenPos);
  if (selected == viewFollowers) {
    showNodeRelationDialog(uid, true);
  } else if (selected == viewFans) {
    showNodeRelationDialog(uid, false);
  }
}

void MainWindow::showNodeRelationDialog(uint64_t uid, bool showFollowers) {
  const QList<uint64_t> relations = showFollowers
      ? m_followersByUid.value(uid)
      : m_fansByUid.value(uid);
  const QString title = showFollowers ? "Followers" : "Fans";
  if (relations.isEmpty()) {
    QMessageBox::information(this,
                             QString("%1 of %2").arg(title).arg(uid),
                             QString("No %1 data available for this node.").arg(title.toLower()));
    return;
  }

  QStringList lines;
  lines.reserve(relations.size());
  for (uint64_t rid : relations) {
    lines.append(QString::number(rid));
  }

  QMessageBox box(this);
  box.setWindowTitle(QString("%1 of %2 (%3)").arg(title).arg(uid).arg(relations.size()));
  box.setText(QString("%1 UID list:").arg(title));
  box.setDetailedText(lines.join("\n"));
  box.setIcon(QMessageBox::Information);
  box.exec();
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
