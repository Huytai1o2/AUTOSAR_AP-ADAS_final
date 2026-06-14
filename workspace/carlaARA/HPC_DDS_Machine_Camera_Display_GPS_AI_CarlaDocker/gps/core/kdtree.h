#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <algorithm>
#include <limits>
#include <cstddef>

#include "traffic_node.h"

class KDTree {
    struct Node {
        TrafficNode point;
        Node* left  = nullptr;
        Node* right = nullptr;
    };

    Node* root = nullptr;

    Node* build(std::vector<TrafficNode>& points, int lo, int hi, int depth) {
        if (lo > hi) return nullptr;

        int axis = depth % 2; // 0 = latitude, 1 = longitude
        int mid = lo + (hi - lo) / 2;

        std::nth_element(points.begin() + lo, points.begin() + mid, points.begin() + hi + 1,
            [axis](const TrafficNode& a, const TrafficNode& b) {
                return axis == 0
                    ? a.coords.latitude < b.coords.latitude
                    : a.coords.longitude < b.coords.longitude;
            });

        Node* node = new Node{points[mid], nullptr, nullptr};
        node->left  = build(points, lo, mid - 1, depth + 1);
        node->right = build(points, mid + 1, hi, depth + 1);
        return node;
    }

    void nearest(Node* node, const Coordinates& target, int depth,
                 const TrafficNode*& best, double& bestDist) const {
        if (!node) return;

        double dist = haversine(target, node->point.coords);
        if (dist < bestDist) {
            bestDist = dist;
            best = &node->point;
        }

        int axis = depth % 2;
        double diff = axis == 0
            ? target.latitude  - node->point.coords.latitude
            : target.longitude - node->point.coords.longitude;

        Node* first  = diff < 0 ? node->left  : node->right;
        Node* second = diff < 0 ? node->right : node->left;

        nearest(first, target, depth + 1, best, bestDist);

        // Approximate axis-aligned distance in meters for pruning:
        // 1 degree latitude  ~ 111,320 m
        // 1 degree longitude ~ 111,320 * cos(lat) m
        double axisDist;
        if (axis == 0)
            axisDist = std::abs(diff) * 111320.0;
        else
            axisDist = std::abs(diff) * 111320.0
                     * std::cos(target.latitude * M_PI / 180.0);

        if (axisDist < bestDist)
            nearest(second, target, depth + 1, best, bestDist);
    }

    void destroy(Node* node) {
        if (!node) return;
        destroy(node->left);
        destroy(node->right);
        delete node;
    }

public:
    KDTree() = default;

    explicit KDTree(std::vector<TrafficNode> points) {
        if (!points.empty())
            root = build(points, 0, static_cast<int>(points.size()) - 1, 0);
    }

    ~KDTree() { destroy(root); }

    KDTree(const KDTree&) = delete;
    KDTree& operator=(const KDTree&) = delete;

    KDTree(KDTree&& other) noexcept : root(other.root) { other.root = nullptr; }
    KDTree& operator=(KDTree&& other) noexcept {
        if (this != &other) { destroy(root); root = other.root; other.root = nullptr; }
        return *this;
    }

    TrafficNode nearest(const Coordinates& target) const {
        if (!root) throw std::runtime_error("KDTree is empty");
        const TrafficNode* best = nullptr;
        double bestDist = std::numeric_limits<double>::max();
        nearest(root, target, 0, best, bestDist);
        return *best;
    }
};
