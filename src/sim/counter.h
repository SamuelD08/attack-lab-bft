//
// Created by sam on 06/11/2026.
//

#ifndef ATTACK_LAB_BFT_COUNTER_H
#define ATTACK_LAB_BFT_COUNTER_H
#include <cstdint>
#include <map>

#include "protocol/message.h"

struct Counters
{
    std::uint64_t sent = 0;
    std::uint64_t dropped = 0;
    std::uint64_t extraCopies = 0;
    std::uint64_t delivered = 0;
    std::uint64_t bytesSent = 0;
    std::map<MessageType, std::uint64_t> sentPerType;
};

class CounterManager
{
private:
    Counters counter;

public:
    [[nodiscard]] Counters getCounters() const;
    void incrementCountersSender(MessageType messageType, std::uint64_t numBytes);
    void incrementExtraCopies();
    void incrementDropMessage();
    void incrementDeliveredMessage();
};


#endif //ATTACK_LAB_BFT_COUNTER_H
