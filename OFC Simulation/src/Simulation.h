// Simulation.h — Top-level wiring: build nodes, dispatch events, write CSVs.

#pragma once

#include "Config.h"
#include "EventScheduler.h"
#include "Metrics.h"
#include "OLT.h"
#include "ONU.h"
#include "ServerLink.h"
#include "TunaExchange.h"

#include <memory>
#include <random>
#include <vector>

class Simulation {
public:
    // Construct scheduler, OLT, ONUs, Server link; load TUNA CSV.
    explicit Simulation(Config cfg)
        : cfg_(std::move(cfg)),
          metrics_(cfg_.N),
          rng_(cfg_.rng_seed) {
        tuna_.loadExchanges(cfg_.tuna_csv);
        link_ = std::make_unique<ServerLink>(&sched_, &cfg_);
        olt_ = std::make_unique<OLT>(&sched_, &metrics_, &cfg_, link_.get(), &tuna_);
        onus_.reserve(cfg_.N);
        for (int i = 0; i < cfg_.N; ++i) {
            onus_.push_back(
                std::make_unique<ONU>(i, &sched_, &metrics_, &cfg_, &rng_));
        }
        sched_.setHandler([this](const Event& e) { dispatch(e); });
    }

    // Init traffic + IPACT, run until sim_time, flush metric CSVs.
    void run() {
        for (auto& onu : onus_) onu->initTraffic();
        olt_->startIPactCycle();
        Event end;
        end.time = cfg_.sim_time;
        end.type = EventType::SIM_END;
        sched_.schedule(end);
        sched_.run(cfg_.sim_time);
        metrics_.writePerOnuCsv(cfg_.metrics_per_onu);
        metrics_.writeNetworkCsv(cfg_.metrics_network, cfg_);
    }

    const Metrics& metrics() const { return metrics_; }
    const Config& config() const { return cfg_; }
    int tunaPollingCount() const { return tuna_.countPollingMessages(); }

private:
    // Route each event type to the owning ONU or OLT handler.
    void dispatch(const Event& e) {
        switch (e.type) {
        case EventType::PKT_ARRIVAL_ONU:
            if (validOnu(e.onu_id)) onus_[e.onu_id]->onPacketArrival(e.pkt);
            break;
        case EventType::GATE_ARRIVE_ONU:
            if (validOnu(e.onu_id)) onus_[e.onu_id]->onGateArrive(e.pkt);
            break;
        case EventType::US_BURST_ARRIVE_OLT:
            olt_->onUsDataArrive(e.pkt);
            break;
        case EventType::DS_DATA_ARRIVE_ONU:
            if (validOnu(e.onu_id)) onus_[e.onu_id]->onDsDataArrive(e.pkt);
            break;
        case EventType::OLT_PROCESS_DONE:
            olt_->onPoissonProcessDone(e.pkt);
            break;
        case EventType::SERVER_MSG_ARRIVE_SERVER:
            olt_->onServerMsgArriveAtServer(e.pkt);
            break;
        case EventType::SERVER_MSG_ARRIVE_OLT:
            olt_->onServerMsgArriveAtOlt(e.pkt);
            break;
        case EventType::TUNA_POLL_TIMER:
            olt_->onPollTimer();
            break;
        case EventType::SIM_END:
            break;
        }
    }

    bool validOnu(int id) const { return id >= 0 && id < cfg_.N; }

    Config cfg_;
    EventScheduler sched_;
    Metrics metrics_;
    TunaExchange tuna_;
    std::unique_ptr<ServerLink> link_;
    std::unique_ptr<OLT> olt_;
    std::vector<std::unique_ptr<ONU>> onus_;
    std::mt19937_64 rng_;
};
