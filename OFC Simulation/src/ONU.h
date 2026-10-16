// ONU.h — Optical Network Unit: traffic generation, IPACT upstream TX, DS RX.
// ONU ids [0, Nserver) emit one Server-A request; others emit Poisson traffic.

#pragma once

#include "Config.h"
#include "EventScheduler.h"
#include "Metrics.h"
#include "Packet.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <random>

class ONU {
public:
    ONU(int id, EventScheduler* sched, Metrics* metrics, const Config* cfg,
        std::mt19937_64* rng)
        : id_(id), sched_(sched), metrics_(metrics), cfg_(cfg), rng_(rng) {
        is_server_ = (id < cfg->Nserver);
        metrics_->setServerOnu(id_, is_server_);
    }

    int id() const { return id_; }
    bool isServerOnu() const { return is_server_; }
    int queueBytes() const { return queue_bytes_; }

    // Schedule initial traffic: one SERVER_REQ at t=0, or first Poisson arrival.
    void initTraffic() {
        if (is_server_) {
            Event e;
            e.time = 0.0;
            e.type = EventType::PKT_ARRIVAL_ONU;
            e.onu_id = id_;
            e.pkt.id = next_pkt_id_++;
            e.pkt.onu_id = id_;
            e.pkt.type = PacketType::SERVER_REQ;
            e.pkt.size = ethernetFrameBytes(cfg_->Dserver);
            e.pkt.t_create = 0.0;
            sched_->schedule(e);
        } else {
            scheduleNextPoisson(sched_->now());
        }
    }

    // Enqueue arriving packet (drop if buffer full); reschedule Poisson if needed.
    void onPacketArrival(const Packet& tmpl) {
        Packet p = tmpl;
        p.t_create = sched_->now();
        if (queue_bytes_ + p.size > cfg_->onu_buffer_bytes) {
            metrics_->recordLoss(id_);
        } else {
            queue_.push_back(p);
            queue_bytes_ += p.size;
            metrics_->recordSent(id_);
        }
        if (!is_server_) {
            scheduleNextPoisson(sched_->now());
        }
    }

    // On GATE: send data up to grant_bytes, append REPORT of remaining queue.
    // Event time is already aligned to grant_start by the OLT scheduler.
    void onGateArrive(const Packet& gate) {
        const double now = sched_->now();
        (void)now;
        int remaining = gate.grant_bytes;
        std::deque<Packet> sent;

        while (!queue_.empty() && remaining > 0) {
            Packet& front = queue_.front();
            if (front.size > remaining) break;
            remaining -= front.size;
            queue_bytes_ -= front.size;
            sent.push_back(front);
            queue_.pop_front();
        }

        // Limited IPACT: report queue length after this grant's transmission.
        Packet report;
        report.id = next_pkt_id_++;
        report.onu_id = id_;
        report.type = PacketType::REPORT;
        report.size = cfg_->report_size;
        report.reported_bytes = queue_bytes_;
        report.t_create = sched_->now();
        sent.push_back(report);

        // Stagger US arrivals by per-packet serialization + fiber delay x_km.
        double cursor = sched_->now();
        for (auto& p : sent) {
            const double pkt_tx = txTime(p.size, cfg_->line_rate_epon);
            cursor += pkt_tx;
            Event e;
            e.time = cursor + propDelay(cfg_->x_km);
            e.type = EventType::US_BURST_ARRIVE_OLT;
            e.onu_id = id_;
            e.pkt = p;
            e.pkt.t_arrive = e.time;
            sched_->schedule(e);
        }
    }

    // Downstream response: record E2E latency for Poisson or Server-A replies.
    void onDsDataArrive(const Packet& p) {
        if (p.onu_id != id_) return;
        if (p.type == PacketType::POISSON_RSP || p.type == PacketType::SERVER_RSP) {
            const double lat = sched_->now() - p.t_create;
            metrics_->recordLatency(id_, lat);
        }
    }

private:
    // Exponential inter-arrival: IAT = -ln(U) / lambda.
    void scheduleNextPoisson(double from) {
        std::uniform_real_distribution<double> U(0.0, 1.0);
        const double u = std::max(1e-12, U(*rng_));
        const double iat = -std::log(u) / cfg_->lambda;
        Event e;
        e.time = from + iat;
        e.type = EventType::PKT_ARRIVAL_ONU;
        e.onu_id = id_;
        e.pkt.id = next_pkt_id_++;
        e.pkt.onu_id = id_;
        e.pkt.type = PacketType::POISSON_REQ;
        e.pkt.size = ethernetFrameBytes(cfg_->poisson_pkt_size);
        e.pkt.t_create = e.time;
        sched_->schedule(e);
    }

    int id_;
    bool is_server_ = false;
    EventScheduler* sched_;
    Metrics* metrics_;
    const Config* cfg_;
    std::mt19937_64* rng_;
    std::deque<Packet> queue_;
    int queue_bytes_ = 0;
    uint64_t next_pkt_id_ = 1;
};
