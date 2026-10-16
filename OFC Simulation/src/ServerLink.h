// ServerLink.h — Full-duplex 10G OLT↔Server A optical link model.
// Each direction has its own serializer (olt_free_ / server_free_).
// Delay per message = serialization + propDelay(y_km).

#pragma once

#include "Config.h"
#include "EventScheduler.h"
#include "Packet.h"

#include <algorithm>

class ServerLink {
public:
    ServerLink(EventScheduler* sched, const Config* cfg)
        : sched_(sched), cfg_(cfg) {}

    // Transmit OLT → Server A; schedule SERVER_MSG_ARRIVE_SERVER on arrival.
    double transmitOltToServer(Packet pkt) {
        const double now = sched_->now();
        const double start = std::max(now, olt_free_);
        const double tx = txTime(pkt.size, cfg_->line_rate_server);
        olt_free_ = start + tx;
        const double arrive = olt_free_ + propDelay(cfg_->y_km);
        Event e;
        e.time = arrive;
        e.type = EventType::SERVER_MSG_ARRIVE_SERVER;
        e.onu_id = pkt.onu_id;
        e.pkt = pkt;
        e.pkt.t_arrive = arrive;
        sched_->schedule(e);
        return arrive;
    }

    // Transmit Server A → OLT; schedule SERVER_MSG_ARRIVE_OLT on arrival.
    double transmitServerToOlt(Packet pkt) {
        const double now = sched_->now();
        const double start = std::max(now, server_free_);
        const double tx = txTime(pkt.size, cfg_->line_rate_server);
        server_free_ = start + tx;
        const double arrive = server_free_ + propDelay(cfg_->y_km);
        Event e;
        e.time = arrive;
        e.type = EventType::SERVER_MSG_ARRIVE_OLT;
        e.onu_id = pkt.onu_id;
        e.pkt = pkt;
        e.pkt.t_arrive = arrive;
        sched_->schedule(e);
        return arrive;
    }

private:
    EventScheduler* sched_;
    const Config* cfg_;
    double olt_free_ = 0.0;    // next time OLT TX can start
    double server_free_ = 0.0; // next time Server TX can start
};
