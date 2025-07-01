#ifndef A_STAR_HPP_
#define A_STAR_HPP_

#include <algorithm>
#include <cmath>
#include <iomanip> // for std::setw
#include <iostream>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

namespace astar {

class Point {
public:
  int x, y;

  Point(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}

  bool operator==(const Point &o) const {
    return x == o.x && y == o.y;
  }

  bool operator!=(const Point &o) const {
    return !(*this == o);
  }

  bool operator<(const Point &o) const {
    return x < o.x || (x == o.x && y < o.y);
  }
};

} // namespace astar

namespace std {
  template <>
  struct hash<astar::Point> {
    size_t operator()(const astar::Point &p) const {
      return hash<long long>()((static_cast<long long>(p.x) << 32) | static_cast<unsigned>(p.y));
    }
  };
}

namespace astar {

class AStar {
  struct Node {
    Point coordinates;
    Node *parent;
    float gCost;
    float hCost;

    float fCost() const { return gCost + hCost; }

    Node(Point coord_, Node *parent_ = nullptr) :
      coordinates(coord_), parent(parent_), gCost(0), hCost(0) {}
  };

public:
  AStar(int width, int height, const Point &start_, const Point &goal_) :
    gridWidth(width), gridHeight(height), start(start_), goal(goal_) {
    grid.resize(gridWidth, std::vector<bool>(gridHeight, true));
  }

  void setWall(int x, int y) {
    if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
      grid[x][y] = false;
    }
  }

  struct CompareNode {
    bool operator()(const Node *lhs, const Node *rhs) const {
      if (lhs->fCost() == rhs->fCost())
        return lhs->hCost > rhs->hCost; // Prefer closer to goal
      return lhs->fCost() > rhs->fCost();
    }
  };

  std::vector<Point> findPath() {
    std::vector<Point> path;
    std::priority_queue<Node *, std::vector<Node *>, CompareNode> openSet;
    std::unordered_map<Point, std::unique_ptr<Node>> allNodes;

    auto startNode = std::make_unique<Node>(start);
    startNode->gCost = 0;
    startNode->hCost = heuristic(start, goal);

    openSet.push(startNode.get());
    allNodes[start] = std::move(startNode);

    while (!openSet.empty()) {
      Node *currentNode = openSet.top();
      openSet.pop();

      if (currentNode->coordinates == goal) {
        while (currentNode != nullptr) {
          path.push_back(currentNode->coordinates);
          currentNode = currentNode->parent;
        }
        break;
      }

      for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
          if (x == 0 && y == 0) continue;

          Point newCoordinates(currentNode->coordinates.x + x, currentNode->coordinates.y + y);

          if (!isInBounds(newCoordinates) || !grid[newCoordinates.x][newCoordinates.y]) continue;

          float newGCost = currentNode->gCost + ((abs(x + y) == 1) ? straightCost : diagonalCost);

          auto found = allNodes.find(newCoordinates);
          if (found != allNodes.end() && found->second->gCost <= newGCost) continue;

          auto successor = std::make_unique<Node>(newCoordinates, currentNode);
          successor->gCost = newGCost;
          successor->hCost = heuristic(successor->coordinates, goal);
          Node* rawPtr = successor.get();

          allNodes[newCoordinates] = std::move(successor);
          // openSet.push(successor.get());
          openSet.push(rawPtr);
        }
      }
    }

    std::reverse(path.begin(), path.end());
    return path;
  }

  bool isWalkable(int x, int y) const {
    if (x >= 0 && x < gridWidth && y >= 0 && y < gridHeight) {
      return grid[x][y];
    }
    return false;
  }

private:
  Point start, goal;
  std::vector<std::vector<bool>> grid;
  int gridWidth, gridHeight;

  const float diagonalCost = std::sqrt(2.0f);
  const float straightCost = 1.0f;

  static float heuristic(const Point &a, const Point &b) {
    float dx = static_cast<float>(a.x - b.x);
    float dy = static_cast<float>(a.y - b.y);
    float dist = std::sqrt(dx * dx + dy * dy);
    return dist;
  }

  bool isInBounds(const Point &p) const {
    return p.x >= 0 && p.x < gridWidth && p.y >= 0 && p.y < gridHeight;
  }
};

} // namespace astar

#endif // A_STAR_HPP_