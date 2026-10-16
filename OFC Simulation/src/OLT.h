// OLT.h — Optical Line Terminal: IPACT DBA, Poisson processing, TUNA sessions.
// Server-A sessions are sequential (one active TUNA sequence at a time).

#pragma once

#include "Config.h"
#include "EventScheduler.h"
#include "Metrics.h"
#include "Packet.h"
#include "ServerLink.h"
#include "TunaExchange.h"

#include <algorithm>
#include <deque>
#include <vector>

class OLT {
public:
    OLT(EventScheduler* sched, Metrics* metrics, const Config* cfg,
        ServerLink* link, const TunaExchange* tuna)
        : sched_(sched),
          metrics_(metrics),
          cfg_(cfg),
          link_(link),
          tuna_(tuna),
          reported_(cfg->N, 0),
          has_report_(cfg->N, false) {}

    // Bootstrap IPACT: GATE ONU 0; round-robin continues on each REPORT.
    void startIPactCycle() {
        const double t = sched_->now();
        const double prop = propDelay(cfg_->x_km);
        Packet gate;
        gate.onu_id = 0;
        gate.type = PacketType::GATE;
        gate.size = cfg_->gate_size;
        gate.grant_bytes = std::max(cfg_->discovery_grant, cfg_->report_size);
        gate.grant_start = t + prop + txTime(cfg_->gate_size, cfg_->line_rate_epon);
        sendGate(0, gate, t);
        const double us_tx = txTime(gate.grant_bytes, cfg_->line_rate_epon);
        channel_free_ = gate.grant_start + us_tx + prop;
    }

    // Upstream arrival: REPORT updates DBA; data is classified by PacketType.
    void onUsDataArrive(const Packet& pkt) {
        if (pkt.type == PacketType::REPORT) {
            onReportArrive(pkt.onu_id, pkt.reported_bytes);
            return;
        }
        classifyAndHandle(pkt);
    }

    // After Tpoisson, send a one-packet reply downstream to the source ONU.
    void onPoissonProcessDone(const Packet& req) {
        Packet rsp;
        rsp.id = req.id;
        rsp.onu_id = req.onu_id;
        rsp.type = PacketType::POISSON_RSP;
        rsp.size = ethernetFrameBytes(cfg_->poisson_reply_size);
        rsp.t_create = req.t_create; // preserve for E2E latency
        sendDownstream(req.onu_id, rsp);
    }

    // Server A path: reply immediately with the paired SERVER_TO_OLT CSV row.
    void onServerMsgArriveAtServer(const Packet& msg) {
        if (msg.tuna_seq < 0 || msg.tuna_seq + 1 >= tuna_->size()) return;
        const auto& reply_row = tuna_->messages()[msg.tuna_seq + 1];
        Packet reply;
        reply.onu_id = msg.onu_id;
        reply.type = PacketType::TUNA;
        reply.size = reply_row.size_bytes;
        reply.tuna_seq = reply_row.sequence;
        reply.tuna_op = reply_row.logical_operation;
        reply.olt_to_server = false;
        reply.t_create = msg.t_create;
        link_->transmitServerToOlt(reply);
    }

    // Advance TUNA sequence; pace list_batch_jobs at poll_interval, else send next.
    void onServerMsgArriveAtOlt(const Packet& msg) {
        if (!session_active_) return;
        tuna_index_ += 2; // finished one OLT↔Server pair
        if (tuna_index_ >= tuna_->size()) {
            finishTunaSession();
            return;
        }
        const auto& next = tuna_->messages()[tuna_index_];
        if (!TunaExchange::isOltToServer(next)) {
            ++tuna_index_;
            onServerMsgArriveAtOlt(msg);
            return;
        }
        if (TunaExchange::isPollingOp(next.logical_operation)) {
            // First poll: send ASAP; later polls: last_poll_send + poll_interval.
            double when;
            if (poll_count_session_ == 0) {
                when = sched_->now();
            } else {
                when = last_poll_send_time_ + cfg_->poll_interval;
                if (when < sched_->now()) when = sched_->now();
            }
            Event e;
            e.time = when;
            e.type = EventType::TUNA_POLL_TIMER;
            e.onu_id = session_onu_;
            poll_timer_id_ = sched_->schedule(e);
        } else {
            sendNextTunaMessage();
        }
    }

    void onPollTimer() {
        if (!session_active_) return;
        sendNextTunaMessage();
    }

private:
    void onReportArrive(int onu_id, int reported_bytes) {
        reported_[onu_id] = reported_bytes;
        has_report_[onu_id] = true;
        // Classic IPACT interleaved polling: grant the next ONU immediately.
        scheduleNextGrant(onu_id);
    }

    // Limited service: grant = min(reported, MaxGrant), at least REPORT size.
    void scheduleNextGrant(int just_reported_onu) {
        const int next = (just_reported_onu + 1) % cfg_->N;
        const int grant = std::min(reported_[next] > 0 ? reported_[next] : 0,
                                   cfg_->MaxGrant);
        const int grant_bytes = std::max(grant, cfg_->report_size);

        const double now = sched_->now();
        const double prop = propDelay(cfg_->x_km);
        const double gate_tx = txTime(cfg_->gate_size, cfg_->line_rate_epon);

        double gate_send = now;
        double gate_arrive = gate_send + gate_tx + prop;
        double us_start = gate_arrive;
        // Avoid upstream collisions: us_start + prop >= channel_free_ + guard.
        const double min_start = channel_free_ - prop + cfg_->guard_time;
        if (us_start < min_start) {
            us_start = min_start;
            gate_arrive = us_start;
            gate_send = gate_arrive - prop - gate_tx;
            if (gate_send < now) {
                gate_send = now;
                gate_arrive = gate_send + gate_tx + prop;
                us_start = gate_arrive;
            }
        }

        Packet gate;
        gate.onu_id = next;
        gate.type = PacketType::GATE;
        gate.size = cfg_->gate_size;
        gate.grant_bytes = grant_bytes;
        gate.grant_start = us_start;
        sendGate(next, gate, gate_send);

        const double us_tx = txTime(grant_bytes, cfg_->line_rate_epon);
        channel_free_ = us_start + us_tx + prop;
    }

    // Deliver GATE to ONU at max(propagation arrival, grant_start).
    void sendGate(int onu_id, Packet gate, double send_time) {
        const double prop = propDelay(cfg_->x_km);
        const double tx = txTime(gate.size, cfg_->line_rate_epon);
        Event e;
        e.time = std::max(send_time, sched_->now()) + tx + prop;
        e.time = std::max(e.time, gate.grant_start);
        e.type = EventType::GATE_ARRIVE_ONU;
        e.onu_id = onu_id;
        e.pkt = gate;
        sched_->schedule(e);
    }

    // Route upstream data: Poisson → process delay; Server → TUNA queue.
    void classifyAndHandle(const Packet& pkt) {
        if (pkt.type == PacketType::POISSON_REQ) {
            Event e;
            e.time = sched_->now() + cfg_->Tpoisson;
            e.type = EventType::OLT_PROCESS_DONE;
            e.onu_id = pkt.onu_id;
            e.pkt = pkt;
            sched_->schedule(e);
        } else if (pkt.type == PacketType::SERVER_REQ) {
            enqueueServerSession(pkt.onu_id, pkt);
        }
    }

    // Downstream EPON transmit with per-direction busy time ds_free_.
    void sendDownstream(int onu_id, Packet pkt) {
        const double now = sched_->now();
        const double start = std::max(now, ds_free_);
        const double tx = txTime(pkt.size, cfg_->line_rate_epon);
        ds_free_ = start + tx;
        Event e;
        e.time = ds_free_ + propDelay(cfg_->x_km);
        e.type = EventType::DS_DATA_ARRIVE_ONU;
        e.onu_id = onu_id;
        e.pkt = pkt;
        e.pkt.t_arrive = e.time;
        sched_->schedule(e);
    }

    void enqueueServerSession(int onu_id, const Packet& pkt) {
        PendingSession s;
        s.onu_id = onu_id;
        s.req = pkt;
        server_q_.push_back(s);
        if (!session_active_) startTunaSession();
    }

    // Start next queued Server-A ONU through the full TUNA CSV sequence.
    void startTunaSession() {
        if (server_q_.empty()) {
            session_active_ = false;
            return;
        }
        PendingSession s = server_q_.front();
        server_q_.pop_front();
        session_active_ = true;
        session_onu_ = s.onu_id;
        session_req_ = s.req;
        tuna_index_ = 0;
        poll_count_session_ = 0;
        last_poll_send_time_ = 0.0;
        sendNextTunaMessage();
    }

    // Send next OLT_TO_SERVER row (skips SERVER_TO_OLT rows if misaligned).
    void sendNextTunaMessage() {
        if (!session_active_ || tuna_index_ >= tuna_->size()) return;
        const auto& row = tuna_->messages()[tuna_index_];
        if (!TunaExchange::isOltToServer(row)) {
            ++tuna_index_;
            sendNextTunaMessage();
            return;
        }
        Packet msg;
        msg.onu_id = session_onu_;
        msg.type = PacketType::TUNA;
        msg.size = row.size_bytes;
        msg.tuna_seq = tuna_index_; // vector index for reply pairing
        msg.tuna_op = row.logical_operation;
        msg.olt_to_server = true;
        msg.t_create = session_req_.t_create;

        if (TunaExchange::isPollingOp(row.logical_operation)) {
            last_poll_send_time_ = sched_->now();
            ++poll_count_session_;
        }
        link_->transmitOltToServer(msg);
    }

    // Sequence done: record polls, send SERVER_RSP to ONU, start next session.
    void finishTunaSession() {
        metrics_->recordPolling(session_onu_, poll_count_session_);
        if (poll_timer_id_ != 0) {
            sched_->cancel(poll_timer_id_);
            poll_timer_id_ = 0;
        }
        Packet rsp;
        rsp.id = session_req_.id;
        rsp.onu_id = session_onu_;
        rsp.type = PacketType::SERVER_RSP;
        rsp.size = ethernetFrameBytes(cfg_->server_reply_size);
        rsp.t_create = session_req_.t_create;
        sendDownstream(session_onu_, rsp);

        session_active_ = false;
        session_onu_ = -1;
        if (!server_q_.empty()) startTunaSession();
    }

    struct PendingSession {
        int onu_id = -1;
        Packet req;
    };

    EventScheduler* sched_;
    Metrics* metrics_;
    const Config* cfg_;
    ServerLink* link_;
    const TunaExchange* tuna_;

    std::vector<int> reported_;     // last reported queue per ONU
    std::vector<char> has_report_;
    double channel_free_ = 0.0;     // upstream channel free time at OLT
    double ds_free_ = 0.0;          // downstream TX free time

    // Active TUNA session state (sequential across Nserver ONUs)
    bool session_active_ = false;
    int session_onu_ = -1;
    Packet session_req_;
    int tuna_index_ = 0;
    int poll_count_session_ = 0;
    double last_poll_send_time_ = 0.0;
    uint64_t poll_timer_id_ = 0;
    std::deque<PendingSession> server_q_;
};
