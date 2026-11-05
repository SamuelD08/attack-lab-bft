//
// Created by sam on 05/11/2026.
//

#ifndef ATTACK_LAB_BFT_TEST_NODE_H
#define ATTACK_LAB_BFT_TEST_NODE_H
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "sim/node.h"
#include "protocol/message.h"
#include "sim/transport.h"

constexpr std::uint64_t kForever = std::numeric_limits<std::uint64_t>::max();

    // One line per delivery, shared by all the nodes of a test, in delivery order.
    using Log = std::vector<std::string>;

inline Message makeMessage(const std::uint32_t from, const std::uint32_t to, const std::uint64_t seq)
    {
        Message m;
        m.sender = from;
        m.receiver = to;
        m.type = MessageType::PREPARE;
        m.seq = seq;
        return m;
    }

    // A node that writes down everything it receives, then (optionally) reacts.
    class TestNode : public Node
    {
    public:
        TestNode(std::uint32_t nodeId, const Transport& network, Log& sharedLog)
            : id(nodeId), transport(network), log(sharedLog)
        {
        }

        void onMessage(const Message& m) override
        {
            log.push_back("t=" + std::to_string(transport.now()) + " node=" + std::to_string(id) +
                          " message seq=" + std::to_string(m.seq) + " from=" + std::to_string(m.sender));
            messages.push_back(m);
            messageTimes.push_back(transport.now());
            if (whenMessage)
            {
                whenMessage(m);
            }
        }

        void onTimer(std::uint64_t timerId) override
        {
            log.push_back("t=" + std::to_string(transport.now()) + " node=" + std::to_string(id) +
                          " timer=" + std::to_string(timerId));
            timerIds.push_back(timerId);
            timerTimes.push_back(transport.now());
            if (whenTimer)
            {
                whenTimer(timerId);
            }
        }

        // Optional reactions, set by a test (for example: reply to the sender).
        std::function<void(const Message&)> whenMessage;
        std::function<void(std::uint64_t)> whenTimer;

        std::vector<Message> messages;
        std::vector<std::uint64_t> messageTimes;
        std::vector<std::uint64_t> timerIds;
        std::vector<std::uint64_t> timerTimes;

    private:
        std::uint32_t id;
        const Transport& transport;
        Log& log;
    };

#endif //ATTACK_LAB_BFT_TEST_NODE_H
