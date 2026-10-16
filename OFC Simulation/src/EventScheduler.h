// EventScheduler.h — Discrete-event kernel (min-heap by event time).

#pragma once

#include "Packet.h"

#include <cstdint>
#include <functional>
#include <queue>
#include <unordered_set>
#include <vector>

class EventScheduler {
public:
    using Handler = std::function<void(const Event&)>;

    // Register the callback invoked for each non-cancelled event.
    void setHandler(Handler h) { handler_ = std::move(h); }

    // Insert event; returns unique id usable with cancel().
    uint64_t schedule(Event e) {
        e.id = next_id_++;
        e.cancelled = false;
        pq_.push(e);
        return e.id;
    }

    // Mark an event so it is skipped when popped (lazy cancellation).
    void cancel(uint64_t event_id) { cancelled_.insert(event_id); }

    // Current simulation time (advanced as events are processed).
    double now() const { return now_; }

    // Process events until time exceeds `until` or SIM_END is reached.
    void run(double until) {
        while (!pq_.empty()) {
            Event e = pq_.top();
            pq_.pop();
            if (cancelled_.count(e.id)) {
                cancelled_.erase(e.id);
                continue;
            }
            if (e.time > until) {
                now_ = until;
                break;
            }
            now_ = e.time;
            if (e.type == EventType::SIM_END) break;
            if (handler_) handler_(e);
        }
    }

    bool empty() const { return pq_.empty(); }

private:
    double now_ = 0.0;
    uint64_t next_id_ = 1;
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> pq_;
    std::unordered_set<uint64_t> cancelled_;
    Handler handler_;
};
