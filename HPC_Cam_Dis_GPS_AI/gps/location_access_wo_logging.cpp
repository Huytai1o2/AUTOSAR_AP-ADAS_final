#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cerrno>
#include <cstring>
#include <sys/select.h>

#include "traffic_node.h"
#include "geojson_parser.h"
#include "kdtree.h"

// ── GPS constants ─────────────────────────────────────────────────────────────
#define GPS_UART_DEV     "/dev/ttyAMA0"
#define GPS_BAUD         B115200
#define GPS_BUF_SIZE     256
#define GPS_TIMEOUT_S    3      // seconds without any byte = sensor lost / Rx dead
#define GPS_RETRY_DELAY  2      // seconds to wait before retrying open()

// ── NMEA helpers ──────────────────────────────────────────────────────────────
static double nmeaToDeg(const std::string& field, char hemi) {
    if (field.empty()) return 0.0;
    double raw = std::stod(field);
    int    deg = static_cast<int>(raw / 100);
    double min = raw - deg * 100.0;
    double result = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') result = -result;
    return result;
}

static std::vector<std::string> splitNmea(const std::string& line) {
    std::vector<std::string> fields;
    std::istringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, ','))
        fields.push_back(tok);
    return fields;
}

static bool parseNmeaFix(const std::string& raw, Coordinates& out) {
    if (raw.size() < 6) return false;

    std::string s = (raw[0] == '$') ? raw.substr(1) : raw;
    auto star = s.rfind('*');
    if (star != std::string::npos) s = s.substr(0, star);

    auto f = splitNmea(s);
    if (f.empty()) return false;

    // $GNGLL / $GPGLL  → field[6] must be 'A'
    if ((f[0] == "GNGLL" || f[0] == "GPGLL") && f.size() >= 7) {
        if (f[6] != "A" || f[1].empty() || f[3].empty()) return false;
        out.latitude  = nmeaToDeg(f[1], f[2].empty() ? 'N' : f[2][0]);
        out.longitude = nmeaToDeg(f[3], f[4].empty() ? 'E' : f[4][0]);
        return true;
    }

    // $GNRMC / $GPRMC  → field[2] must be 'A'
    if ((f[0] == "GNRMC" || f[0] == "GPRMC") && f.size() >= 7) {
        if (f[2] != "A" || f[3].empty() || f[5].empty()) return false;
        out.latitude  = nmeaToDeg(f[3], f[4].empty() ? 'N' : f[4][0]);
        out.longitude = nmeaToDeg(f[5], f[6].empty() ? 'E' : f[6][0]);
        return true;
    }

    // $GPGGA / $GNGGA  → field[6] quality != '0'
    if ((f[0] == "GPGGA" || f[0] == "GNGGA") && f.size() >= 7) {
        if (f[6] == "0" || f[2].empty() || f[4].empty()) return false;
        out.latitude  = nmeaToDeg(f[2], f[3].empty() ? 'N' : f[3][0]);
        out.longitude = nmeaToDeg(f[4], f[5].empty() ? 'E' : f[5][0]);
        return true;
    }

    return false;
}

// ── UART open ─────────────────────────────────────────────────────────────────
// Returns a valid fd on success, -1 on failure (error printed to stderr).
static int openGPSUART() {
    int fd = open(GPS_UART_DEV, O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        std::cerr << "[GPS] cannot open " << GPS_UART_DEV
                  << ": " << strerror(errno) << "\n";
        return -1;
    }

    struct termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "[GPS] tcgetattr: " << strerror(errno) << "\n";
        close(fd);
        return -1;
    }
    cfmakeraw(&tty);
    cfsetispeed(&tty, GPS_BAUD);
    tty.c_cc[VMIN]  = 0;   // use select() for timeout-aware blocking
    tty.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "[GPS] tcsetattr: " << strerror(errno) << "\n";
        close(fd);
        return -1;
    }
    return fd;
}

// ── GPS read ──────────────────────────────────────────────────────────────────
// Reads lines from an open fd until a valid NMEA fix is parsed.
// Returns true + fix on success.
// Returns false when:
//   - read() returns 0  → EOF / Rx line disconnected
//   - read() returns <0 → I/O error
//   - select() times out → GPS_TIMEOUT_S seconds of silence (power lost / signal lost)
static bool readNextGPSFix(int fd, Coordinates& fix) {
    char buf[GPS_BUF_SIZE];
    char line[GPS_BUF_SIZE];
    int  linePos = 0;

    while (true) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv{ GPS_TIMEOUT_S, 0 };

        int ret = select(fd + 1, &rfds, nullptr, nullptr, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[GPS] select error: " << strerror(errno) << "\n";
            return false;
        }
        if (ret == 0) {
            // No bytes for GPS_TIMEOUT_S seconds → sensor silent
            std::cerr << "[GPS] timeout: no data for " << GPS_TIMEOUT_S
                      << " s (power lost or Rx disconnected)\n";
            return false;
        }

        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) {
            // EOF: Rx line physically disconnected (HUP)
            std::cerr << "[GPS] EOF on " << GPS_UART_DEV
                      << " (Rx disconnected)\n";
            return false;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            std::cerr << "[GPS] read error: " << strerror(errno) << "\n";
            return false;
        }

        for (ssize_t i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\n') {
                line[linePos] = '\0';
                linePos = 0;
                if (parseNmeaFix(std::string(line), fix))
                    return true;
            } else if (c != '\r' && linePos < GPS_BUF_SIZE - 1) {
                line[linePos++] = c;
            }
        }
    }
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_geojson>\n";
        return 1;
    }

    // Parse GeoJSON and build KD-tree once
    std::vector<TrafficNode> nodes;
    try {
        nodes = parseGeoJSON(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "Failed to load GeoJSON: " << e.what() << "\n";
        return 1;
    }
    if (nodes.empty()) {
        std::cerr << "GeoJSON contains no traffic nodes.\n";
        return 1;
    }

    KDTree tree(nodes);

    // Open UART — retry forever if the device is absent
    int gpsFd = -1;
    while (gpsFd < 0) {
        gpsFd = openGPSUART();
        if (gpsFd < 0) {
            std::cerr << "[GPS] retrying in " << GPS_RETRY_DELAY << " s...\n";
            sleep(GPS_RETRY_DELAY);
        }
    }
    std::cerr << "[GPS] " << GPS_UART_DEV << " open. Waiting for fix...\n";

    // Main loop — never exits; only Ctrl-C / SIGTERM stops the program
    while (true) {
        // ── GPS read ──────────────────────────────────────────────────────
        Coordinates gpsCoord{};
        if (!readNextGPSFix(gpsFd, gpsCoord)) {
            close(gpsFd);
            gpsFd = -1;
            std::cerr << "[GPS] connection lost — reconnecting...\n";
            while (gpsFd < 0) {
                sleep(GPS_RETRY_DELAY);
                gpsFd = openGPSUART();
                if (gpsFd < 0)
                    std::cerr << "[GPS] device not available, retrying in "
                              << GPS_RETRY_DELAY << " s...\n";
            }
            std::cerr << "[GPS] reconnected. Waiting for fix...\n";
            continue;
        }

        // ── Nearest-neighbor query ────────────────────────────────────────
        try {
            TrafficNode result = tree.nearest(gpsCoord);
            double dist = haversine(gpsCoord, result.coords);

            std::cout << std::fixed
                      << "id="   << result.id
                      << " lat=" << std::setprecision(7) << result.coords.latitude
                      << " lon=" << result.coords.longitude
                      << " dist=" << std::setprecision(1) << dist << "m\n";
            std::cout.flush();
        } catch (const std::exception& e) {
            std::cerr << "[KD] query error: " << e.what() << " — skipping\n";
        }
    }
}
