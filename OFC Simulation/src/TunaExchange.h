// TunaExchange.h — Load and query the OLT↔Server A message sequence CSV.
// Expected columns: sequence,logical_operation,direction,size_bytes

#pragma once

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// One row from tuna-v1-4-exchanges.csv.
struct TunaMessage {
    int sequence = 0;
    std::string logical_operation; // e.g. oauth_discovery, list_batch_jobs
    std::string direction;         // OLT_TO_SERVER or SERVER_TO_OLT
    int size_bytes = 0;
};

class TunaExchange {
public:
    // Parse CSV into an ordered list of request/response pairs.
    void loadExchanges(const std::string& csv_path) {
        messages_.clear();
        std::ifstream in(csv_path);
        if (!in) {
            throw std::runtime_error("Failed to open TUNA CSV: " + csv_path);
        }
        std::string line;
        std::getline(in, line); // skip header
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string seq_s, op, dir, size_s;
            if (!std::getline(ss, seq_s, ',')) continue;
            if (!std::getline(ss, op, ',')) continue;
            if (!std::getline(ss, dir, ',')) continue;
            if (!std::getline(ss, size_s, ',')) continue;
            TunaMessage m;
            m.sequence = std::stoi(seq_s);
            m.logical_operation = op;
            m.direction = dir;
            m.size_bytes = std::stoi(size_s);
            messages_.push_back(m);
        }
        if (messages_.empty()) {
            throw std::runtime_error("No TUNA exchanges loaded from " + csv_path);
        }
    }

    const std::vector<TunaMessage>& messages() const { return messages_; }
    int size() const { return static_cast<int>(messages_.size()); }

    // Polling ops are paced every poll_interval (default 0.5 s) from OLT to Server.
    static bool isPollingOp(const std::string& op) {
        return op == "list_batch_jobs";
    }

    static bool isOltToServer(const TunaMessage& m) {
        return m.direction == "OLT_TO_SERVER";
    }

    // Number of OLT→Server list_batch_jobs (polling metric).
    int countPollingMessages() const {
        int n = 0;
        for (const auto& m : messages_) {
            if (isPollingOp(m.logical_operation) && isOltToServer(m)) ++n;
        }
        return n;
    }

private:
    std::vector<TunaMessage> messages_;
};
