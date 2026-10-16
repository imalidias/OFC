// Config.h — Simulation parameters and optical-link delay helpers.
// Topology: N ONUs at x_km from OLT; OLT linked to Server A at y_km (10 Gbps).

#pragma once

#include <cstdint>
#include <string>

// Tunable run parameters (CLI in main.cpp overrides these defaults).
struct Config {
    int N = 8;                  // total ONUs
    int Nserver = 5;            // first Nserver ONUs send one-shot Server-A traffic
    double x_km = 20.0;         // ONU–OLT fiber distance (km)
    double y_km = 100.0;        // OLT–Server A fiber distance (km)
    int Dserver = 1500;         // Server-A request payload (bytes)
    double Tpoisson = 1e-6;     // OLT processing time for Poisson requests (s)
    double lambda = 1365.0;     // Poisson arrival rate per ONU (1/s)
    double sim_time = 2800.0;   // simulation duration (s); ~526 s per TUNA poll phase × Nserver
    int MaxGrant = 15000;       // max upstream grant bytes (limited IPACT)
    double guard_time = 1.5e-6; // inter-ONU upstream guard time (s)
    double line_rate_epon = 10e9;   // 10G EPON line rate (bps)
    double line_rate_server = 10e9; // OLT–Server A link rate (bps)
    int onu_buffer_bytes = 1 << 20; // ONU queue capacity (1 MiB)
    int poisson_pkt_size = 1500;    // Poisson request payload (bytes)
    int poisson_reply_size = 64;    // Poisson response payload (bytes)
    int server_reply_size = 64;     // Server-A completion reply payload (bytes)
    int gate_size = 64;             // MPCP GATE size (bytes)
    int report_size = 64;           // MPCP REPORT size (bytes)
    int discovery_grant = 64;       // bootstrap grant (enough for REPORT)
    double poll_interval = 0.5;     // list_batch_jobs polling period (s)
    uint64_t rng_seed = 1;
    std::string tuna_csv = "tuna-v1-4-exchanges.csv";
    std::string metrics_per_onu = "metrics_per_onu.csv";
    std::string metrics_network = "metrics_network.csv";
};

// Effective speed of light in fiber (~2e8 m/s).
inline constexpr double C_FIBER = 2.0e8;

// One-way propagation delay for a fiber span of given length (km).
inline double propDelay(double km) {
    return (km * 1000.0) / C_FIBER;
}

// Serialization (transmission) time for a frame of given size at rate_bps.
inline double txTime(int bytes, double rate_bps) {
    return (static_cast<double>(bytes) * 8.0) / rate_bps;
}

// Payload → on-wire Ethernet frame size (preamble+SFD 8 + HDR 14 + FCS 4).
inline int ethernetFrameBytes(int payload) {
    return payload + 26;
}
