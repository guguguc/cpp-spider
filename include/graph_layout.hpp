#ifndef GRAPH_LAYOUT_HPP
#define GRAPH_LAYOUT_HPP

#include <QMap>
#include <QPointF>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <limits>

enum class LayoutType {
    Random,
    Circular,
    ForceDirected,
    KamadaKawai,
    Grid,
    Hierarchical
};

struct GraphData {
    QMap<uint64_t, QPointF> positions;
    QMap<uint64_t, std::vector<uint64_t>> adjacency;
};

class GraphLayout {
public:
    static QPointF randomLayout(const QMap<uint64_t, QPointF>& positions, uint64_t seed = 42) {
        static std::mt19937 gen(seed);
        std::uniform_real_distribution<double> dist(-500.0, 500.0);
        return QPointF(dist(gen), dist(gen));
    }

    static QMap<uint64_t, QPointF> applyLayout(
        LayoutType type,
        const QMap<uint64_t, QPointF>& positions,
        const QMap<uint64_t, std::vector<uint64_t>>& adjacency,
        int width = 1000,
        int height = 1000) {
        
        switch (type) {
            case LayoutType::Random:
                return randomLayoutAll(positions);
            case LayoutType::Circular:
                return circularLayout(positions, width, height);
            case LayoutType::ForceDirected:
                return forceDirectedLayout(positions, adjacency, width, height);
            case LayoutType::KamadaKawai:
                return kamadaKawaiLayout(positions, adjacency, width, height);
            case LayoutType::Grid:
                return gridLayout(positions, width, height);
            case LayoutType::Hierarchical:
                return hierarchicalLayout(positions, adjacency, width, height);
            default:
                return positions;
        }
    }

private:
    static QMap<uint64_t, QPointF> randomLayoutAll(const QMap<uint64_t, QPointF>& positions) {
        std::mt19937 gen(42);
        std::uniform_real_distribution<double> distX(-500.0, 500.0);
        std::uniform_real_distribution<double> distY(-500.0, 500.0);
        
        QMap<uint64_t, QPointF> result;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            result[it.key()] = QPointF(distX(gen), distY(gen));
        }
        return result;
    }

    static QMap<uint64_t, QPointF> circularLayout(
        const QMap<uint64_t, QPointF>& positions,
        int width, int height) {
        
        QMap<uint64_t, QPointF> result;
        int count = positions.size();
        if (count == 0) return result;

        double centerX = width / 2.0;
        double centerY = height / 2.0;
        double radius = std::min(width, height) / 2.0 - 50;

        int i = 0;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            double angle = 2.0 * M_PI * i / count;
            result[it.key()] = QPointF(
                centerX + radius * std::cos(angle),
                centerY + radius * std::sin(angle)
            );
            i++;
        }
        return result;
    }

    static QMap<uint64_t, QPointF> forceDirectedLayout(
        const QMap<uint64_t, QPointF>& positions,
        const QMap<uint64_t, std::vector<uint64_t>>& adjacency,
        int width, int height) {
        
        QMap<uint64_t, QPointF> result = randomLayoutAll(positions);
        QList<uint64_t> vertices = result.keys();
        int n = vertices.size();
        if (n == 0) return result;

        double k = std::sqrt((width * height) / static_cast<double>(n));
        double temp = 100.0;
        double cooling = 0.95;
        int iterations = 100;

        for (int iter = 0; iter < iterations && temp > 0.1; iter++) {
            QMap<uint64_t, QPointF> displacement;
            for (const auto& v : vertices) {
                displacement[v] = QPointF(0, 0);
            }

            for (int i = 0; i < n; i++) {
                for (int j = i + 1; j < n; j++) {
                    uint64_t vi = vertices[i];
                    uint64_t vj = vertices[j];
                    QPointF diff = result[vi] - result[vj];
                    double dist = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
                    if (dist < 0.1) dist = 0.1;

                    double repulsion = k * k / dist;
                    QPointF force = diff / dist * repulsion;
                    
                    displacement[vi] = displacement[vi] + force;
                    displacement[vj] = displacement[vj] - force;
                }
            }

            for (const auto& vi : vertices) {
                const auto& neighbors = adjacency.value(vi);
                for (uint64_t vj : neighbors) {
                    if (!result.contains(vj)) continue;
                    QPointF diff = result[vj] - result[vi];
                    double dist = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
                    if (dist < 0.1) dist = 0.1;

                    double attraction = dist * dist / k;
                    QPointF force = diff / dist * attraction;
                    
                    displacement[vi] = displacement[vi] + force;
                }
            }

            for (const auto& v : vertices) {
                QPointF disp = displacement[v];
                double dispLen = std::sqrt(disp.x() * disp.x() + disp.y() * disp.y());
                if (dispLen > 0.1) {
                    disp = disp / dispLen * std::min(dispLen, temp);
                }
                result[v] = result[v] + disp;

                result[v].setX(std::max(0.0, std::min(static_cast<double>(width), result[v].x())));
                result[v].setY(std::max(0.0, std::min(static_cast<double>(height), result[v].y())));
            }

            temp *= cooling;
        }

        centerGraph(result, width, height);
        return result;
    }

    static QMap<uint64_t, QPointF> kamadaKawaiLayout(
        const QMap<uint64_t, QPointF>& positions,
        const QMap<uint64_t, std::vector<uint64_t>>& adjacency,
        int width, int height) {
        
        QMap<uint64_t, QPointF> result = randomLayoutAll(positions);
        QList<uint64_t> vertices = result.keys();
        int n = vertices.size();
        if (n == 0) return result;

        double L = std::sqrt((width * height) / static_cast<double>(n));
        
        QMap<uint64_t, QMap<uint64_t, double>> pathLengths;
        for (const auto& vi : vertices) {
            pathLengths[vi][vi] = 0.0;
        }

        for (const auto& vi : vertices) {
            const auto& neighbors = adjacency.value(vi);
            for (uint64_t vj : neighbors) {
                if (vertices.contains(vj)) {
                    pathLengths[vi][vj] = 1.0;
                    pathLengths[vj][vi] = 1.0;
                }
            }
        }

        for (const auto& k : vertices) {
            for (const auto& i : vertices) {
                if (i == k) continue;
                for (const auto& j : vertices) {
                    if (i == j || j == k) continue;
                    double newLen = pathLengths[i][k] + pathLengths[k][j];
                    if (pathLengths[i][j] > newLen) {
                        pathLengths[i][j] = newLen;
                        pathLengths[j][i] = newLen;
                    }
                }
            }
        }

        for (const auto& i : vertices) {
            for (const auto& j : vertices) {
                if (pathLengths[i][j] == 0.0 && i != j) {
                    pathLengths[i][j] = std::numeric_limits<double>::infinity();
                }
            }
        }

        double optimalDist = L;
        int iterations = 50;

        for (int iter = 0; iter < iterations; iter++) {
            for (int i = 0; i < n; i++) {
                uint64_t vi = vertices[i];
                QPointF sum(0, 0);
                double totalWeight = 0.0;

                for (int j = 0; j < n; j++) {
                    if (i == j) continue;
                    uint64_t vj = vertices[j];
                    double Lij = pathLengths[vi][vj];
                    if (Lij == 0 || std::isinf(Lij)) Lij = 1.0;
                    
                    double kij = optimalDist / Lij;
                    QPointF diff = result[vj] - result[vi];
                    double dist = std::sqrt(diff.x() * diff.x() + diff.y() * diff.y());
                    if (dist < 0.1) dist = 0.1;

                    sum = sum + diff / dist * kij;
                    totalWeight += kij;
                }

                if (totalWeight > 0.1) {
                    result[vi] = result[vi] + sum / totalWeight;
                }
            }
        }

        centerGraph(result, width, height);
        return result;
    }

    static QMap<uint64_t, QPointF> gridLayout(
        const QMap<uint64_t, QPointF>& positions,
        int width, int height) {
        
        QMap<uint64_t, QPointF> result;
        int count = positions.size();
        if (count == 0) return result;

        int cols = static_cast<int>(std::ceil(std::sqrt(count)));
        int cellWidth = width / cols;
        int cellHeight = height / std::max(1, (count + cols - 1) / cols);

        int i = 0;
        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            int row = i / cols;
            int col = i % cols;
            result[it.key()] = QPointF(
                col * cellWidth + cellWidth / 2.0,
                row * cellHeight + cellHeight / 2.0
            );
            i++;
        }
        return result;
    }

    static QMap<uint64_t, QPointF> hierarchicalLayout(
        const QMap<uint64_t, QPointF>& positions,
        const QMap<uint64_t, std::vector<uint64_t>>& adjacency,
        int width, int height) {
        
        QMap<uint64_t, QPointF> result;
        QList<uint64_t> vertices = positions.keys();
        if (vertices.isEmpty()) return result;

        QMap<uint64_t, int> levels;
        QMap<uint64_t, bool> visited;
        
        uint64_t root = vertices.first();
        levels[root] = 0;
        
        std::vector<uint64_t> queue;
        queue.push_back(root);
        visited[root] = true;

        while (!queue.empty()) {
            uint64_t current = queue.front();
            queue.erase(queue.begin());
            
            int currentLevel = levels[current];
            const auto& neighbors = adjacency.value(current);
            
            for (uint64_t neighbor : neighbors) {
                if (!visited.contains(neighbor)) {
                    levels[neighbor] = currentLevel + 1;
                    visited[neighbor] = true;
                    queue.push_back(neighbor);
                }
            }
        }

        for (const auto& v : vertices) {
            if (!levels.contains(v)) {
                levels[v] = 2;
            }
        }

        QMap<int, QList<uint64_t>> levelMap;
        for (auto it = levels.constBegin(); it != levels.constEnd(); ++it) {
            levelMap[it.value()].append(it.key());
        }

        int maxLevel = 0;
        for (auto it = levelMap.constBegin(); it != levelMap.constEnd(); ++it) {
            if (it.key() > maxLevel) maxLevel = it.key();
        }

        int levelHeight = height / (maxLevel + 1);

        for (auto it = levelMap.constBegin(); it != levelMap.constEnd(); ++it) {
            int level = it.key();
            const QList<uint64_t>& nodesAtLevel = it.value();
            int count = nodesAtLevel.size();
            int levelWidth = width / (count + 1);
            
            for (int i = 0; i < count; i++) {
                result[nodesAtLevel[i]] = QPointF(
                    (i + 1) * levelWidth,
                    level * levelHeight + levelHeight / 2.0
                );
            }
        }

        return result;
    }

    static void centerGraph(QMap<uint64_t, QPointF>& positions, int width, int height) {
        if (positions.isEmpty()) return;

        double minX = std::numeric_limits<double>::max();
        double maxX = std::numeric_limits<double>::lowest();
        double minY = std::numeric_limits<double>::max();
        double maxY = std::numeric_limits<double>::lowest();

        for (auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            minX = std::min(minX, it.value().x());
            maxX = std::max(maxX, it.value().x());
            minY = std::min(minY, it.value().y());
            maxY = std::max(maxY, it.value().y());
        }

        double graphWidth = maxX - minX;
        double graphHeight = maxY - minY;
        if (graphWidth < 1.0) graphWidth = 1.0;
        if (graphHeight < 1.0) graphHeight = 1.0;

        double scale = std::min((width - 100) / graphWidth, (height - 100) / graphHeight);
        double centerX = width / 2.0 - (minX + maxX) / 2.0 * scale;
        double centerY = height / 2.0 - (minY + maxY) / 2.0 * scale;

        for (auto it = positions.begin(); it != positions.end(); ++it) {
            it.value() = QPointF(it.value().x() * scale + centerX, it.value().y() * scale + centerY);
        }
    }
};

#endif
