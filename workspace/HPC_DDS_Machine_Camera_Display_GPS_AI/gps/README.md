# KD-tree Traffic Light Nearest-Neighbor Finder

Finds the nearest traffic signal node from Ho Chi Minh City's OpenStreetMap data using a custom 2D KD-tree with Haversine distance.

## Dependencies

- **C++17** compiler (g++, clang++, or MSVC)
- **nlohmann/json** (`json.hpp`, included in the project)

## Build

```bash
g++ -std=c++17 -O2 -o location_access location_access.cpp
```

## Run

Place `hochiminh.geojson` in the same directory, then:

```bash
./location_access
```

## Files

| File | Description |
|---|---|
| `location_access.cpp` | Main program — parses GeoJSON, builds KD-tree, runs nearest-neighbor query, lists all nodes |
| `traffic_node.h` | `Coordinates` and `TrafficNode` structs, Haversine distance function |
| `geojson_parser.h` | Header-only GeoJSON parser |
| `kdtree.h` | Header-only 2D KD-tree implementation |
| `json.hpp` | nlohmann/json single-header library |
| `hochiminh.geojson` | Traffic signal data from OpenStreetMap |
