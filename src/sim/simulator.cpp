//
// Created by sam on 04/11/2026.
//
#include "simulator.h"
#include <map>

Simulator::Simulator(const std::uint64_t seed, const std::uint64_t baseDelay, const std::uint64_t maxExtraDelay): randomGenerator(seed), baseDelay(baseDelay), maxExtraDelay(maxExtraDelay){}

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

    // Make an Event
    Event event;
    event.time = clock + baseDelay + extra;
    event.order = nextOrder++;
    event.target = m.receiver;
    event.kind = EventKind::Delivery;
    event.message = m;

    // Push the event into the queue
    queue.push(event);
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
            continue;
        }
        if (event.kind == EventKind::Delivery)
        {
            node->second->onMessage(event.message);
        } else
        {
            node->second->onTimer(event.timerId);
        }
    }
}


