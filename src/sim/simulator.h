//
// Created by sam on 04/11/2026.
//

#ifndef ATTACK_LAB_BFT_SIMULATOR_H
#define ATTACK_LAB_BFT_SIMULATOR_H
#include <cstdint>
#include <queue>
#include <random>
#include <tuple>
#include <vector>
#include <map>

#include "counter.h"
#include "node.h"
#include "deliveryPolicy.h"
#include "transport.h"

enum class EventKind
{
    Delivery,
    Timer
};

struct Event
{
    std::uint64_t time = 0; // in microseconds
    std::uint64_t order = 0;
    std::uint32_t target = 0; // which node receives it
    EventKind kind = EventKind::Delivery;
    Message message; // used when the kind is Delivery
    std::uint64_t timerId = 0; // used when the kind is Timer
};

struct Later
{
    bool operator()(const Event& a, const Event& b) const
    {
        return std::tie(a.time, a.order) > std::tie(b.time, b.order);
    }
};

class Simulator: public Transport
{
private:
    std::priority_queue<Event, std::vector<Event>, Later> queue;
    std::uint64_t clock = 0;

    std::mt19937_64 randomGenerator;

    const std::uint64_t baseDelay;
    const std::uint64_t maxExtraDelay;

    //Nodes
    std::map<std::uint32_t, Node*> nodes;

    std::uint64_t nextOrder = 0;

    DeliveryPolicy* networkPolicy = nullptr;
    CounterManager counterManager;


public:
    Simulator(std::uint64_t seed, std::uint64_t baseDelay, std::uint64_t maxExtraDelay);

    void send(const Message& m) override;
    void setTimer(std::uint32_t nodeId, std::uint64_t delay, std::uint64_t timerId) override;
    [[nodiscard]] std::uint64_t now() const override;

    void addNode(std::uint32_t id, Node& node);
    void run(std::uint64_t timeLimit);

    void setNetworkPolicy(DeliveryPolicy& policy);

    [[nodiscard]] Counters getCounters() const;
};

#endif //ATTACK_LAB_BFT_SIMULATOR_H
