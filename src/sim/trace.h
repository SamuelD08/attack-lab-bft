//
// Created by sam on 06/11/2026.
//

#ifndef ATTACK_LAB_BFT_TRACE_H
#define ATTACK_LAB_BFT_TRACE_H
#include <cstdint>

#include "protocol/message.h"

enum class TraceKind
{
    Sent,
    Dropped,
    Delivered,
    Undeliverable,
    TimerFired
};

struct Record
{
    std::uint64_t time = 0;
    TraceKind kind = TraceKind::Sent;
    std::uint64_t messageId = 0;
    std::uint64_t sender = 0;
    std::uint64_t receiver = 0;
    MessageType type = MessageType::REQUEST;
    std::uint64_t view = 0;
    std::uint64_t seq = 0;
    std::uint64_t timerId = 0; // only for TimerFired

    bool operator==(const Record&) const = default;


};

std::string toString(const Record& record);
std::ostream& operator<<(std::ostream& out, const Record& record);


class Trace
{
private:
    std::vector<Record> records;
    void addMessageRecord(std::uint64_t time, TraceKind kind, std::uint64_t messageId, const Message& m);
    bool enabled = true;


public:
    void recordSent(std::uint64_t time, std::uint64_t messageId, const Message& m);
    void recordDropped(std::uint64_t time, std::uint64_t messageId, const Message& m);
    void recordDelivered(std::uint64_t time, std::uint64_t messageId, const Message& m);
    void recordUndeliverable(std::uint64_t time, std::uint64_t messageId, const Message& m);
    void recordTimerFired(std::uint64_t time, std::uint32_t nodeId, std::uint64_t timerId);

    // Nothing recorded when disabled
    void setEnabled(bool on);
    [[nodiscard]] bool isEnabled() const;

    // Prints one line per entry. The caller choose where (std::cout or a file for example)
    void print(std::ostream& out) const;

    [[nodiscard]] const std::vector<Record>& getRecords() const;
};


#endif //ATTACK_LAB_BFT_TRACE_H
