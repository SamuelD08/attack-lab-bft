//
// Created by sam on 06/11/2026.
//

#include "counter.h"

Counters CounterManager::getCounters() const
{
    return counter;
}

void CounterManager::incrementCountersSender(MessageType messageType, std::uint64_t numBytes)
{
    counter.sent++;
    counter.bytesSent += numBytes;
    counter.sentPerType.insert_or_assign(messageType, ++counter.sentPerType[messageType]);
}

void CounterManager::incrementExtraCopies()
{
    counter.extraCopies++;
}

void CounterManager::incrementDropMessage()
{
    counter.dropped++;
}

void CounterManager::incrementDeliveredMessage()
{
    counter.delivered++;
}
