// Packet.h — Packet and discrete-event type definitions for the EPON simulator.

#pragma once

#include <cstdint>
#include <string>

// Application / control traffic carried on the EPON or OLT–Server link.
enum class PacketType {
    SERVER_REQ,   // one-shot request from Nserver ONU toward Server A
    POISSON_REQ,  // continuous Poisson request processed at OLT
    POISSON_RSP,  // single-packet reply after Tpoisson processing
    SERVER_RSP,   // single-packet reply after TUNA sequence completes
    GATE,         // MPCP GATE (downstream bandwidth grant)
    REPORT,       // MPCP REPORT (upstream queue advertisement)
    TUNA          // OLT↔Server A exchange message from tuna CSV
};

// Events processed by EventScheduler (priority queue ordered by time).
enum class EventType {
    PKT_ARRIVAL_ONU,           // local traffic generated at an ONU
    GATE_ARRIVE_ONU,           // GATE reaches ONU (at/after grant start)
    US_BURST_ARRIVE_OLT,       // upstream data or REPORT arrives at OLT
    DS_DATA_ARRIVE_ONU,        // downstream data arrives at ONU
    OLT_PROCESS_DONE,          // Poisson processing delay finished
    SERVER_MSG_ARRIVE_SERVER,  // TUNA message arrives at Server A
    SERVER_MSG_ARRIVE_OLT,     // TUNA reply arrives at OLT
    TUNA_POLL_TIMER,           // poll_interval timer for next list_batch_jobs
    SIM_END                    // stop simulation and flush metrics
};

// Generic packet / control message carried in events.
struct Packet {
    uint64_t id = 0;
    int onu_id = -1;
    PacketType type = PacketType::POISSON_REQ;
    int size = 0;           // bytes on the wire
    double t_create = 0.0;  // creation time (used for E2E latency)
    double t_arrive = 0.0;
    int dest = -1;

    // GATE fields
    double grant_start = 0.0; // absolute time ONU may begin upstream TX
    int grant_bytes = 0;      // granted upstream window (bytes)

    // REPORT field: remaining queue after this transmission (limited IPACT)
    int reported_bytes = 0;

    // TUNA fields
    int tuna_seq = -1;          // index into TunaExchange message vector
    std::string tuna_op;        // logical_operation from CSV
    bool olt_to_server = true;
};

// Scheduled simulation event. operator> supports min-heap via std::greater.
struct Event {
    double time = 0.0;
    EventType type = EventType::SIM_END;
    uint64_t id = 0;       // unique id for cancel()
    int onu_id = -1;
    Packet pkt;
    bool cancelled = false;

    bool operator>(const Event& other) const {
        if (time != other.time) return time > other.time;
        return id > other.id;
    }
};
