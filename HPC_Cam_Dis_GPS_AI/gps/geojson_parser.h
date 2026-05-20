#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>

#include "json.hpp"
#include "traffic_node.h"

inline std::vector<TrafficNode> parseGeoJSON(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: " + filepath);

    nlohmann::json doc = nlohmann::json::parse(file);
    std::vector<TrafficNode> nodes;
    nodes.reserve(doc["features"].size());

    for (auto& feature : doc["features"]) {
        TrafficNode node;

        // Extract numeric ID from "node/366053189" -> 366053189
        std::string idStr = feature["id"].get<std::string>();
        auto pos = idStr.find('/');
        node.id = std::stoll(idStr.substr(pos + 1));

        // GeoJSON coordinates are [longitude, latitude]
        auto& coords = feature["geometry"]["coordinates"];
        node.coords.longitude = coords[0].get<double>();
        node.coords.latitude  = coords[1].get<double>();

        nodes.push_back(node);
    }

    return nodes;
}
