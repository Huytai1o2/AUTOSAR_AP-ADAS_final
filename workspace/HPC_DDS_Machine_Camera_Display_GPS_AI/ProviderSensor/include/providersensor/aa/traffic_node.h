#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

struct Coordinates {
    double latitude;
    double longitude;
};

struct TrafficNode {
    long long id;
    Coordinates coords;
};

inline double haversine(const Coordinates& a, const Coordinates& b) {
    constexpr double R = 6371000.0; // Earth radius in meters
    constexpr double TO_RAD = M_PI / 180.0;

    double dLat = (b.latitude - a.latitude) * TO_RAD;
    double dLon = (b.longitude - a.longitude) * TO_RAD;

    double lat1 = a.latitude * TO_RAD;
    double lat2 = b.latitude * TO_RAD;

    double h = std::sin(dLat / 2) * std::sin(dLat / 2)
             + std::cos(lat1) * std::cos(lat2)
             * std::sin(dLon / 2) * std::sin(dLon / 2);

    return 2.0 * R * std::asin(std::sqrt(h));
}
