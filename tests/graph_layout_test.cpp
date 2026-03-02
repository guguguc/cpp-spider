#include "graph_layout.hpp"

#include <gtest/gtest.h>

namespace {

QMap<uint64_t, QPointF> make_positions(std::initializer_list<uint64_t> ids) {
  QMap<uint64_t, QPointF> positions;
  for (uint64_t id : ids) {
    positions[id] = QPointF(0.0, 0.0);
  }
  return positions;
}

QMap<uint64_t, std::vector<uint64_t>> make_empty_adjacency(std::initializer_list<uint64_t> ids) {
  QMap<uint64_t, std::vector<uint64_t>> adjacency;
  for (uint64_t id : ids) {
    adjacency[id] = {};
  }
  return adjacency;
}

}

TEST(GraphLayoutTest, RandomLayoutIsDeterministicAcrossCalls) {
  const auto positions = make_positions({1, 2, 3, 4});
  const auto adjacency = make_empty_adjacency({1, 2, 3, 4});

  const auto first = GraphLayout::applyLayout(LayoutType::Random, positions, adjacency, 1000, 800);
  const auto second = GraphLayout::applyLayout(LayoutType::Random, positions, adjacency, 1000, 800);

  ASSERT_EQ(first.size(), second.size());
  for (auto it = first.constBegin(); it != first.constEnd(); ++it) {
    ASSERT_TRUE(second.contains(it.key()));
    EXPECT_DOUBLE_EQ(second[it.key()].x(), it.value().x());
    EXPECT_DOUBLE_EQ(second[it.key()].y(), it.value().y());
  }
}

TEST(GraphLayoutTest, CircularLayoutPlacesNodesOnExpectedCircle) {
  const auto positions = make_positions({1, 2, 3, 4});
  const auto adjacency = make_empty_adjacency({1, 2, 3, 4});

  const auto laidOut = GraphLayout::applyLayout(LayoutType::Circular, positions, adjacency, 1000, 1000);

  ASSERT_EQ(laidOut.size(), 4);
  EXPECT_NEAR(laidOut[1].x(), 950.0, 1e-6);
  EXPECT_NEAR(laidOut[1].y(), 500.0, 1e-6);
  EXPECT_NEAR(laidOut[2].x(), 500.0, 1e-6);
  EXPECT_NEAR(laidOut[2].y(), 950.0, 1e-6);
}

TEST(GraphLayoutTest, GridLayoutStaysInsideCanvas) {
  const auto positions = make_positions({1, 2, 3, 4, 5});
  const auto adjacency = make_empty_adjacency({1, 2, 3, 4, 5});

  const auto laidOut = GraphLayout::applyLayout(LayoutType::Grid, positions, adjacency, 640, 480);

  ASSERT_EQ(laidOut.size(), 5);
  for (auto it = laidOut.constBegin(); it != laidOut.constEnd(); ++it) {
    EXPECT_GE(it.value().x(), 0.0);
    EXPECT_LE(it.value().x(), 640.0);
    EXPECT_GE(it.value().y(), 0.0);
    EXPECT_LE(it.value().y(), 480.0);
  }
}

TEST(GraphLayoutTest, HierarchicalLayoutAssignsRootToTopLevel) {
  const auto positions = make_positions({10, 20, 30, 40});
  QMap<uint64_t, std::vector<uint64_t>> adjacency;
  adjacency[10] = {20, 30};
  adjacency[20] = {40};
  adjacency[30] = {};
  adjacency[40] = {};

  const auto laidOut = GraphLayout::applyLayout(LayoutType::Hierarchical, positions, adjacency, 800, 600);

  ASSERT_EQ(laidOut.size(), 4);
  EXPECT_LT(laidOut[10].y(), laidOut[20].y());
  EXPECT_LT(laidOut[10].y(), laidOut[30].y());
}
