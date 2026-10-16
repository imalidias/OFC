// main.cpp — CLI entry point for the 10G EPON + Server A discrete-event simulator.
//
// Example:
//   ./epon_sim --N 8 --Nserver 2 --x 20 --y 100 --sim-time 2200
//
// Outputs:
//   metrics_per_onu.csv   — per-ONU latency, loss, polling counts
//   metrics_network.csv   — network aggregates and run parameters

#include "Simulation.h"

#include <cstdlib>
#include <iostream>
#include <string>

static void printUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [options]\n"
        << "  --N <int>              Total ONUs (default 8)\n"
        << "  --Nserver <int>        Server-A ONUs (default 5)\n"
        << "  --x <km>               ONU-OLT distance km (default 20)\n"
        << "  --y <km>               OLT-Server distance km (default 100)\n"
        << "  --Dserver <bytes>      Server-A payload (default 1500)\n"
        << "  --Tpoisson <s>         Poisson processing time (default 1e-6)\n"
        << "  --lambda <1/s>         Poisson rate per ONU (default 1365)\n"
        << "  --sim-time <s>         Simulation duration (default 2800)\n"
        << "  --MaxGrant <bytes>     IPACT max grant (default 15000)\n"
        << "  --tuna <path>          TUNA exchanges CSV\n"
        << "  --out-onu <path>       Per-ONU metrics CSV\n"
        << "  --out-net <path>       Network metrics CSV\n"
        << "  --seed <uint>          RNG seed\n";
}

static bool parseInt(const std::string& s, int& v) {
    try {
        v = std::stoi(s);
        return true;
    } catch (...) {
        return false;
    }
}

static bool parseDouble(const std::string& s, double& v) {
    try {
        v = std::stod(s);
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char** argv) {
    Config cfg;
    // Defaults also live in Config.h; keep these aligned for CLI runs.
    cfg.Nserver = 5;
    cfg.lambda = 1365.0;
    cfg.sim_time = 2800.0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto need = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << name << "\n";
                std::exit(1);
            }
            return argv[++i];
        };
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--N") {
            if (!parseInt(need("--N"), cfg.N)) return 1;
        } else if (arg == "--Nserver") {
            if (!parseInt(need("--Nserver"), cfg.Nserver)) return 1;
        } else if (arg == "--x") {
            if (!parseDouble(need("--x"), cfg.x_km)) return 1;
        } else if (arg == "--y") {
            if (!parseDouble(need("--y"), cfg.y_km)) return 1;
        } else if (arg == "--Dserver") {
            if (!parseInt(need("--Dserver"), cfg.Dserver)) return 1;
        } else if (arg == "--Tpoisson") {
            if (!parseDouble(need("--Tpoisson"), cfg.Tpoisson)) return 1;
        } else if (arg == "--lambda") {
            if (!parseDouble(need("--lambda"), cfg.lambda)) return 1;
        } else if (arg == "--sim-time") {
            if (!parseDouble(need("--sim-time"), cfg.sim_time)) return 1;
        } else if (arg == "--MaxGrant") {
            if (!parseInt(need("--MaxGrant"), cfg.MaxGrant)) return 1;
        } else if (arg == "--tuna") {
            cfg.tuna_csv = need("--tuna");
        } else if (arg == "--out-onu") {
            cfg.metrics_per_onu = need("--out-onu");
        } else if (arg == "--out-net") {
            cfg.metrics_network = need("--out-net");
        } else if (arg == "--seed") {
            try {
                cfg.rng_seed = std::stoull(need("--seed"));
            } catch (...) {
                return 1;
            }
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (cfg.N <= 0 || cfg.Nserver < 0 || cfg.Nserver > cfg.N) {
        std::cerr << "Invalid N / Nserver\n";
        return 1;
    }

    try {
        std::cout << "10G EPON + Server A discrete-event simulation\n"
                  << "  N=" << cfg.N << " Nserver=" << cfg.Nserver
                  << " x=" << cfg.x_km << "km y=" << cfg.y_km << "km\n"
                  << "  Dserver=" << cfg.Dserver << " Tpoisson=" << cfg.Tpoisson
                  << " lambda=" << cfg.lambda << " sim_time=" << cfg.sim_time
                  << "s\n"
                  << "  tuna=" << cfg.tuna_csv << "\n";

        Simulation sim(cfg);
        std::cout << "  TUNA polling messages in CSV: "
                  << sim.tunaPollingCount() << "\n";
        sim.run();

        std::cout << "Wrote " << cfg.metrics_per_onu << " and "
                  << cfg.metrics_network << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
