// Metrics.h — Per-ONU and network-wide latency, loss, and polling statistics.
// Latency = t_response_arrive_ONU − t_request_create_ONU.

#pragma once

#include "Config.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

class Metrics {
public:
    explicit Metrics(int N) : N_(N) {
        sent_.assign(N, 0);
        recv_.assign(N, 0);
        loss_.assign(N, 0);
        sum_lat_.assign(N, 0.0);
        max_lat_.assign(N, 0.0);
        polling_per_onu_.assign(N, 0);
        is_server_.assign(N, false);
    }

    void setServerOnu(int onu_id, bool v) {
        if (onu_id >= 0 && onu_id < N_) is_server_[onu_id] = v;
    }

    void recordSent(int onu_id) {
        if (valid(onu_id)) ++sent_[onu_id];
    }

    void recordLoss(int onu_id) {
        if (valid(onu_id)) ++loss_[onu_id];
    }

    // Record a successfully completed request/response round-trip.
    void recordLatency(int onu_id, double latency) {
        if (!valid(onu_id)) return;
        ++recv_[onu_id];
        sum_lat_[onu_id] += latency;
        max_lat_[onu_id] = std::max(max_lat_[onu_id], latency);
        total_sum_lat_ += latency;
        ++total_recv_;
        network_max_lat_ = std::max(network_max_lat_, latency);
    }

    // Count OLT→Server list_batch_jobs polls for a finished TUNA session.
    void recordPolling(int onu_id, int count) {
        if (valid(onu_id)) polling_per_onu_[onu_id] += count;
        total_polling_ += count;
    }

    double avgLatency(int onu_id) const {
        if (!valid(onu_id) || recv_[onu_id] == 0) return 0.0;
        return sum_lat_[onu_id] / static_cast<double>(recv_[onu_id]);
    }

    double maxLatency(int onu_id) const {
        return valid(onu_id) ? max_lat_[onu_id] : 0.0;
    }

    // Loss rate = losses / (sent + losses); sent counts accepted enqueues.
    double lossRate(int onu_id) const {
        if (!valid(onu_id)) return 0.0;
        const long long denom = sent_[onu_id] + loss_[onu_id];
        if (denom == 0) return 0.0;
        return static_cast<double>(loss_[onu_id]) / static_cast<double>(denom);
    }

    // Write per-ONU metrics CSV.
    void writePerOnuCsv(const std::string& path) const {
        std::ofstream out(path);
        out << "onu_id,traffic_type,avg_latency,max_latency,packet_loss,"
               "packets_sent,packets_recv,polling_msgs\n";
        for (int i = 0; i < N_; ++i) {
            out << i << ','
                << (is_server_[i] ? "server" : "poisson") << ','
                << avgLatency(i) << ','
                << maxLatency(i) << ','
                << lossRate(i) << ','
                << sent_[i] << ','
                << recv_[i] << ','
                << polling_per_onu_[i] << '\n';
        }
    }

    // Write network-aggregate metrics CSV (includes key Config fields).
    void writeNetworkCsv(const std::string& path, const Config& cfg) const {
        std::ofstream out(path);
        out << "avg_latency,max_latency,packet_loss,total_polling_msgs,"
               "sim_time,N,Nserver,x_km,y_km\n";
        const double avg = (total_recv_ > 0)
            ? total_sum_lat_ / static_cast<double>(total_recv_)
            : 0.0;
        long long total_sent = 0, total_loss = 0;
        for (int i = 0; i < N_; ++i) {
            total_sent += sent_[i];
            total_loss += loss_[i];
        }
        const long long denom = total_sent + total_loss;
        const double loss = (denom > 0)
            ? static_cast<double>(total_loss) / static_cast<double>(denom)
            : 0.0;
        out << avg << ','
            << network_max_lat_ << ','
            << loss << ','
            << total_polling_ << ','
            << cfg.sim_time << ','
            << cfg.N << ','
            << cfg.Nserver << ','
            << cfg.x_km << ','
            << cfg.y_km << '\n';
    }

private:
    bool valid(int onu_id) const { return onu_id >= 0 && onu_id < N_; }

    int N_;
    std::vector<long long> sent_, recv_, loss_, polling_per_onu_;
    std::vector<double> sum_lat_, max_lat_;
    std::vector<char> is_server_;
    double total_sum_lat_ = 0.0;
    double network_max_lat_ = 0.0;
    long long total_recv_ = 0;
    long long total_polling_ = 0;
};
