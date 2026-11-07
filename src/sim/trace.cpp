//
// Created by sam on 06/11/2026.
//

#include "trace.h"

#include <iomanip>
#include <sstream>

namespace
{
    const char* kindName(TraceKind kind)
    {
        switch (kind)
        {
        case TraceKind::Sent:
            return "SENT";
        case TraceKind::Dropped:
            return "DROPPED";
        case TraceKind::Delivered:
            return "DELIVERED";
        case TraceKind::Undeliverable:
            return "UNDELIVERABLE";
        case TraceKind::TimerFired:
            return "TIMER_FIRED";
        }
        return "?";
    }


    const char* typeName(MessageType type)
    {
        switch (type)
        {
        case MessageType::REQUEST:
            return "REQUEST";
        case MessageType::PRE_PREPARE:
            return "PRE_PREPARE";
        case MessageType::PREPARE:
            return "PREPARE";
        case MessageType::COMMIT:
            return "COMMIT";
        case MessageType::REPLY:
            return "REPLY";
        }
        return "?";
    }
}

std::string toString(const Record& record)
{
    std::ostringstream out;
    out << std::left << "t=" << std::setw(8) << record.time;
    if (record.kind == TraceKind::TimerFired)
    {
        out << std::setw(9) << "timer" << std::setw(15) << kindName(record.kind)
            << "node " << record.receiver << ", timer " << record.timerId;
        return out.str();
    }
    out << std::setw(9) << ("msg#" + std::to_string(record.messageId))
        << std::setw(15) << kindName(record.kind)
        << std::setw(13) << typeName(record.type)
        << "view=" << record.view << " seq=" << record.seq << "  "
        << record.sender << " -> " << record.receiver;
    return out.str();
}

std::ostream& operator<<(std::ostream& out, const Record& record)
{
    return out << toString(record);
}

void Trace::recordSent(std::uint64_t time, std::uint64_t messageId, const Message& m)
{
    addMessageRecord(time, TraceKind::Sent, messageId, m);
}

void Trace::recordDelivered(std::uint64_t time, std::uint64_t messageId, const Message& m)
{
    addMessageRecord(time, TraceKind::Delivered, messageId, m);
}

void Trace::recordDropped(std::uint64_t time, std::uint64_t messageId, const Message& m)
{
    addMessageRecord(time, TraceKind::Dropped, messageId, m);
}

void Trace::recordUndeliverable(std::uint64_t time, std::uint64_t messageId, const Message& m)
{
    addMessageRecord(time, TraceKind::Undeliverable, messageId, m);
}

void Trace::recordTimerFired(std::uint64_t time, std::uint32_t nodeId, std::uint64_t timerId)
{
    if (!enabled)
    {
        return;
    }
    Record record;
    record.time = time;
    record.kind = TraceKind::TimerFired;
    record.receiver = nodeId;
    record.timerId = timerId;
    records.push_back(record);
}



void Trace::setEnabled(bool on)
{
    enabled = on;
}

bool Trace::isEnabled() const
{
    return enabled;
}


void Trace::addMessageRecord(std::uint64_t time, TraceKind kind, std::uint64_t messageId, const Message& m)
{
    if (!enabled)
    {
        return;
    }
    Record record;
    record.time = time;
    record.kind = kind;
    record.messageId = messageId;
    record.sender = m.sender;
    record.receiver = m.receiver;
    record.type = m.type;
    record.view = m.view;
    record.seq = m.seq;
    records.push_back(record);
}

void Trace::print(std::ostream& out) const
{
    for (auto record : records)
    {
        out << toString(record) << "\n";
    }
}

const std::vector<Record>& Trace::getRecords() const
{
    return records;
}
