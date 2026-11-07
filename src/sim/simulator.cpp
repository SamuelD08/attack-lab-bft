//
// Created by sam on 04/11/2026.
//
#include "simulator.h"

Simulator::Simulator(const std::uint64_t seed, const std::uint64_t baseDelay, const std::uint64_t maxExtraDelay) :
    randomGenerator(seed), baseDelay(baseDelay), maxExtraDelay(maxExtraDelay)
{

}

void Simulator::setTimer(const std::uint32_t nodeId, const std::uint64_t delay, const std::uint64_t timerId)
{
    // Make an event
    Event event;
    event.time = clock + delay;
    event.order = nextOrder++;
    event.target = nodeId;
    event.kind = EventKind::Timer;
    event.timerId = timerId;

    // Push the event into the queue
    queue.push(event);
}

void Simulator::send(const Message& m)
{
    // Choose a random extra delay
    std::uniform_int_distribution<std::uint64_t> distrib(0, maxExtraDelay);
    const std::uint64_t extra = distrib(randomGenerator);

    // Increment Counter -> POV: Sender
    const std::uint64_t messageId = nextMessageId++;
    counterManager.incrementCountersSender(m.type, serialize(m).size());
    tracer.recordSent(clock, messageId, m);

    // Network change part
    std::vector<std::uint64_t> networkDecision = {0};
    if (networkPolicy != nullptr)
    {
        networkDecision = networkPolicy->decide(m, now());
    }
    // Increment Counter -> POV: Network
    if (networkDecision.empty())
    {
        counterManager.incrementDropMessage();
        tracer.recordDropped(clock, messageId, m);
        return;
    }

    // Make an Event
    Event event;
    event.target = m.receiver;
    event.kind = EventKind::Delivery;
    event.message = m;
    event.messageId = messageId;

    // Push the event into the queue
    for (std::size_t copy = 0; copy < networkDecision.size(); copy++)
    {
        if (copy > 0)
        {
            counterManager.incrementExtraCopies();
        }
        event.order = nextOrder++;
        event.time = clock + baseDelay + extra + networkDecision[copy];
        queue.push(event);
    }
}

std::uint64_t Simulator::now() const
{
    return clock;
}

void Simulator::addNode(const std::uint32_t id, Node& node)
{
    nodes.insert_or_assign(id, &node);
}

void Simulator::run(std::uint64_t timeLimit)
{
    while (!queue.empty() && queue.top().time <= timeLimit)
    {
        auto event = queue.top();
        queue.pop();
        clock = event.time;
        auto node = nodes.find(event.target);
        if (node == nodes.end())
        {
            // TRACE for Undelivered
            if (event.kind == EventKind::Delivery)
            {
                tracer.recordUndeliverable(clock, event.messageId, event.message);
            }
            continue;
        }
        if (event.kind == EventKind::Delivery)
        {
            // Increment Counter -> POV: Network
            counterManager.incrementDeliveredMessage();
            tracer.recordDelivered(clock, event.messageId, event.message);
            node->second->onMessage(event.message);
        } else
        {
            tracer.recordTimerFired(clock, event.target, event.timerId);
            node->second->onTimer(event.timerId);
        }
    }
}

void Simulator::setNetworkPolicy(DeliveryPolicy& policy)
{
    networkPolicy = &policy;
}

Counters Simulator::getCounters() const
{
    return counterManager.getCounters();
}

void Simulator::printTrace(std::ostream& out) const
{
    tracer.print(out);
}

const std::vector<Record>& Simulator::getTrace() const
{
    return tracer.getRecords();
}

void Simulator::setTraceEnabled(bool on)
{
    tracer.setEnabled(on);
}

