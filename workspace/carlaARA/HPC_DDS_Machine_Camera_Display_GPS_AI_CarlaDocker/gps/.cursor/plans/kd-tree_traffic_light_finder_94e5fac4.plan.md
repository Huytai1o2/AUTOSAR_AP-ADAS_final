---
name: KD-tree Traffic Light Finder
overview: Build a C++ program that parses ~50,000 traffic light nodes from a GeoJSON file, stores them in a vector, implements Haversine distance calculation, and uses a KD-tree for nearest-neighbor lookup from a given location.
todos:
  - id: download-json-lib
    content: Download nlohmann/json single-header (json.hpp) into the project directory
    status: pending
  - id: create-traffic-node-h
    content: Create traffic_node.h with Coordinates struct, TrafficNode struct, and haversine() function
    status: pending
  - id: create-geojson-parser-h
    content: Create geojson_parser.h to parse the GeoJSON file into std::vector<TrafficNode>
    status: pending
  - id: create-kdtree-h
    content: Create kdtree.h with 2D KD-tree build and nearest-neighbor search
    status: pending
  - id: create-main-cpp
    content: Write location_access.cpp with main() demonstrating parse, nearest query, and list-all
    status: pending
  - id: create-readme
    content: Write README.md with build instructions and dependency notes
    status: pending
isProject: false
---

# KD-tree Traffic Light Nearest-Neighbor Finder

## Data Structure

- `Coordinates` struct: `double latitude`, `double longitude`
- `TrafficNode` struct: `long long id`, `Coordinates coords`
- Storage: `std::vector<TrafficNode>` -- best for ~50,000 nodes due to contiguous memory (cache-friendly), O(1) random access, and efficient iteration for listing

## GeoJSON Parsing

**Important:** GeoJSON `coordinates` field is `[longitude, latitude]` (not lat-first).

- Parse `id` field: extract numeric suffix from `"node/366053189"` -> `366053189LL`
- Parse `geometry.coordinates[0]` -> longitude, `geometry.coordinates[1]` -> latitude
- External dependency: **nlohmann/json** (single header-only library, `json.hpp`)

## Distance Calculation

- Haversine formula to compute great-circle distance in **meters** between two `Coordinates` on Earth's surface
- Signature: `double haversine(const Coordinates& a, const Coordinates& b)`

## KD-tree (Nearest Neighbor Search)

- Custom 2D KD-tree implementation (no external library)
- Alternates splitting on latitude (dimension 0) and longitude (dimension 1)
- Supports `nearest(Coordinates query)` returning the closest `TrafficNode`
- Build time: O(n log n), query time: O(log n) average
- Distance metric within the tree uses Haversine for geographic accuracy

## List All Nodes

- Function `listAllNodes(const std::vector<TrafficNode>& nodes)` that prints every node's ID, latitude, and longitude in a formatted table

## File Structure

All files in `F:\workspace\doanktmt\`:

- `[location_access.cpp](location_access.cpp)` -- main program with demo: parses file, builds KD-tree, runs a nearest-neighbor query, and lists nodes
- `traffic_node.h` -- `Coordinates` and `TrafficNode` structs, Haversine function
- `geojson_parser.h` -- header-only GeoJSON parser using nlohmann/json
- `kdtree.h` -- header-only KD-tree implementation
- `json.hpp` -- nlohmann/json single-header library (downloaded)
- `README.md` -- build/install instructions, dependency notes

## Compilation

Single command (no build system needed):

```bash
g++ -std=c++17 -O2 -o location_access location_access.cpp
```

The program will read `hochiminh.geojson` from the current directory by default.