#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <cmath>

// POSIX UART (from read_gps.c)
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

#include "traffic_node.h"
#include "geojson_parser.h"
#include "kdtree.h"

namespace fs = std::filesystem;

// ── Timing helper ────────────────────────────────────────────────────────────
using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

static inline double elapsedMs(Clock::time_point t0) {
    return Ms(Clock::now() - t0).count();
}

// ── Logger ───────────────────────────────────────────────────────────────────
std::string fileTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::tm buf{};
    localtime_r(&t, &buf);
    std::ostringstream ss;
    ss << std::put_time(&buf, "%Y%m%d_%H%M%S");
    return ss.str();
}

class Logger {
    std::ofstream file;
public:
    explicit Logger(const std::string& path) : file(path) {
        if (!file.is_open())
            throw std::runtime_error("Cannot create log file: " + path);
    }

    template<typename... Args>
    void log(Args&&... args) {
        std::ostringstream msg;
        (msg << ... << std::forward<Args>(args));
        std::string text = msg.str();
        std::cout << text << "\n";
        file << text << "\n";
        file.flush();
    }
};

// ── NMEA helpers (integrated from read_gps.c) ────────────────────────────────
#define GPS_UART_DEV  "/dev/ttyAMA0"
#define GPS_BAUD      B9600
#define GPS_BUF_SIZE  256

// Convert NMEA ddmm.mmmm to decimal degrees.
static double nmeaToDeg(const std::string& field, char hemi) {
    if (field.empty()) return 0.0;
    double raw = std::stod(field);
    int    deg = static_cast<int>(raw / 100);
    double min = raw - deg * 100.0;
    double result = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') result = -result;
    return result;
}

// Split a comma-separated NMEA sentence into fields.
static std::vector<std::string> splitNmea(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, ','))
        fields.push_back(tok);
    return fields;
}

// Try to extract lat/lon from a GNGLL or GNRMC sentence.
// Returns true and fills coords on success; returns false otherwise.
static bool parseNmeaFix(const std::string& line, Coordinates& out) {
    if (line.size() < 6) return false;

    // Strip leading '$' if present
    std::string s = (line[0] == '$') ? line.substr(1) : line;

    // Strip checksum (*XX) at the end
    auto star = s.rfind('*');
    if (star != std::string::npos) s = s.substr(0, star);

    auto f = splitNmea(s);
    if (f.empty()) return false;

    // $GNGLL,lat,N/S,lon,E/W,time,status,mode
    if ((f[0] == "GNGLL" || f[0] == "GPGLL") && f.size() >= 7) {
        if (f[6] != "A") return false;     // not active
        if (f[1].empty() || f[3].empty()) return false;
        out.latitude  = nmeaToDeg(f[1], f[2].empty() ? 'N' : f[2][0]);
        out.longitude = nmeaToDeg(f[3], f[4].empty() ? 'E' : f[4][0]);
        return true;
    }

    // $GNRMC,time,status,lat,N/S,lon,E/W,...
    if ((f[0] == "GNRMC" || f[0] == "GPRMC") && f.size() >= 7) {
        if (f[2] != "A") return false;     // not active
        if (f[3].empty() || f[5].empty()) return false;
        out.latitude  = nmeaToDeg(f[3], f[4].empty() ? 'N' : f[4][0]);
        out.longitude = nmeaToDeg(f[5], f[6].empty() ? 'E' : f[6][0]);
        return true;
    }

    // $GPGGA,time,lat,N/S,lon,E/W,quality,...
    if ((f[0] == "GPGGA" || f[0] == "GNGGA") && f.size() >= 7) {
        if (f[6] == "0") return false;     // no fix
        if (f[2].empty() || f[4].empty()) return false;
        out.latitude  = nmeaToDeg(f[2], f[3].empty() ? 'N' : f[3][0]);
        out.longitude = nmeaToDeg(f[4], f[5].empty() ? 'E' : f[5][0]);
        return true;
    }

    return false;
}

// Open and configure the UART. Returns the file descriptor.
static int openGPSUART() {
    int fd = open(GPS_UART_DEV, O_RDONLY | O_NOCTTY);
    if (fd < 0)
        throw std::runtime_error(std::string("[GPS] open failed: ") + strerror(errno));

    struct termios tty{};
    if (tcgetattr(fd, &tty) != 0) { close(fd); throw std::runtime_error(std::string("[GPS] tcgetattr: ") + strerror(errno)); }
    cfmakeraw(&tty);
    cfsetispeed(&tty, GPS_BAUD);
    tty.c_cc[VMIN]  = 1;   // block until at least 1 byte arrives
    tty.c_cc[VTIME] = 0;   // no timeout — block forever
    if (tcsetattr(fd, TCSANOW, &tty) != 0) { close(fd); throw std::runtime_error(std::string("[GPS] tcsetattr: ") + strerror(errno)); }

    return fd;
}

// Block until the next valid NMEA fix arrives on an already-open fd.
// Returns the fix and the time taken in gpsMs.
static Coordinates readNextGPSFix(int fd, double& gpsMs) {
    auto tGps = Clock::now();

    char buf[GPS_BUF_SIZE];
    char line[GPS_BUF_SIZE];
    int  linePos = 0;
    Coordinates fix{};

    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(std::string("[GPS] read failed: ") + strerror(errno));
        }
        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\n') {
                line[linePos] = '\0';
                linePos = 0;
                if (parseNmeaFix(std::string(line), fix)) {
                    gpsMs = elapsedMs(tGps);
                    return fix;
                }
            } else if (c != '\r' && linePos < GPS_BUF_SIZE - 1) {
                line[linePos++] = c;
            }
        }
    }
}

// ── Misc helpers ─────────────────────────────────────────────────────────────
void listAllNodes(const std::vector<TrafficNode>& nodes, Logger& logger) {
    std::ostringstream out;
    out << std::left
        << std::setw(16) << "ID"
        << std::setw(14) << "Latitude"
        << std::setw(14) << "Longitude" << "\n";
    out << std::string(44, '-') << "\n";
    out << std::fixed << std::setprecision(7);
    for (auto& n : nodes) {
        out << std::left
            << std::setw(16) << n.id
            << std::setw(14) << n.coords.latitude
            << std::setw(14) << n.coords.longitude << "\n";
    }
    out << "Total: " << nodes.size() << " nodes";
    logger.log(out.str());
}

// ── main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "Usage: " << argv[0] << " <path_to_geojson> [--no-gps]\n";
            return 1;
        }

        const char* geojsonPath = argv[1];
        bool useGPS = true;
        for (int i = 2; i < argc; ++i)
            if (std::string(argv[i]) == "--no-gps") useGPS = false;

        // ── Logging setup ─────────────────────────────────────────────────
        fs::create_directories("logs");
        std::string logPath = "logs/log_" + fileTimestamp() + ".txt";
        Logger logger(logPath);
        logger.log("=== location_access started ===");
        logger.log("Log file : ", logPath);
        logger.log("GeoJSON  : ", geojsonPath);
        logger.log("GPS mode : ", useGPS ? "live UART" : "hardcoded fallback");

        auto tTotal = Clock::now();

        // ── 1. Parse GeoJSON ──────────────────────────────────────────────
        logger.log("\n[1] Parsing GeoJSON...");
        auto tParse = Clock::now();
        auto nodes  = parseGeoJSON(geojsonPath);
        double parseMs = elapsedMs(tParse);
        {
            std::ostringstream s;
            s << std::fixed << std::setprecision(2)
              << "    Loaded " << nodes.size()
              << " traffic signal nodes in " << parseMs << " ms.";
            logger.log(s.str());
        }

        // ── 2. Build KD-tree ──────────────────────────────────────────────
        logger.log("\n[2] Building KD-tree...");
        auto tBuild = Clock::now();
        KDTree tree(nodes);
        double buildMs = elapsedMs(tBuild);
        {
            std::ostringstream s;
            s << std::fixed << std::setprecision(2)
              << "    KD-tree built in " << buildMs << " ms.";
            logger.log(s.str());
        }

        // ── 3. Open UART once (kept open for the whole run) ───────────────
        int gpsFd = -1;
        if (useGPS) {
            logger.log("\n[3] Opening GPS UART ", GPS_UART_DEV, "...");
            gpsFd = openGPSUART();
            logger.log("    UART open. Blocking until satellite fix...");
        }

        // ── 4. Main loop: read GPS fix → nearest-neighbor → log ───────────
        logger.log("\n[4] Entering main loop (Ctrl-C to stop)...\n");
        unsigned long iteration = 0;

        while (true) {
            ++iteration;
            auto tIter = Clock::now();

            Coordinates gpsCoord{};
            double gpsMs = 0.0;

            if (useGPS) {
                // Blocks until a valid GNRMC/GNGLL/GPGGA sentence with a fix
                gpsCoord = readNextGPSFix(gpsFd, gpsMs);
            } else {
                // Fallback: cycle through hardcoded coordinates
                static const std::vector<Coordinates> fallback = {
                    {10.795450955668784, 106.67817762013125},
                    {10.797815913841896, 106.67327418561207},
                    {10.78641929669587,  106.690499013281  },
                    {10.801785619529598, 106.64962134144064},
                    {10.797660948822434, 106.69079713282414},
                    {10.801001008828065, 106.68199843116118},
                    {10.799582410072722, 106.70649749764307},
                    {10.795117792155189, 106.68285987783243},
                };
                gpsCoord = fallback[(iteration - 1) % fallback.size()];
            }

            auto tQ = Clock::now();
            TrafficNode result = tree.nearest(gpsCoord);
            double queryMs = elapsedMs(tQ);
            double dist    = haversine(gpsCoord, result.coords);
            double iterMs  = elapsedMs(tIter);

            std::ostringstream s;
            s << std::fixed
              << "[" << iteration << "] "
              << "GPS(" << std::setprecision(7) << gpsCoord.latitude
              << ", " << gpsCoord.longitude << ")"
              << " -> ID=" << result.id
              << "  loc=(" << result.coords.latitude
              << ", " << result.coords.longitude << ")"
              << "  dist=" << std::setprecision(1) << dist << " m"
              << "  gps=" << std::setprecision(2) << gpsMs << " ms"
              << "  kd=" << std::setprecision(4) << queryMs << " ms"
              << "  iter=" << std::setprecision(2) << iterMs << " ms";
            logger.log(s.str());
        }

        if (gpsFd >= 0) close(gpsFd);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
