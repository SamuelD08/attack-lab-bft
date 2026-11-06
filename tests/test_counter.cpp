//
// Created by sam on 06/11/2026.
//
// Simple tests for the simulator's counters.
//
// The rules these tests check:
//  - sent, bytesSent and sentByType are counted at send(), for every message,
//    even if the network drops it (the sender paid for it).
//  - dropped: the policy returned {} for that message.
//  - delivered: onMessage() was really called (a duplicate counts twice).
//  - A message to an unknown node is sent but never delivered (and not "dropped").
//  - Timers change no counter.
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "sim/deliveryPolicy.h"
#include "sim/simulator.h"
#include "test_node.h" // TestNode, Log, makeMessage, kForever

namespace
{
    // Always gives the same answer: {} = drop, {0, 0} = duplicate.
    class FixedPolicy : public DeliveryPolicy
    {
    private:
        std::vector<std::uint64_t> delays;

    public:
        explicit FixedPolicy(std::vector<std::uint64_t> answer) : delays(answer) {}

        std::vector<std::uint64_t> decide(const Message&, std::uint64_t) override
        {
            return delays;
        }
    };

    // How many messages of one type were sent.
    // If you store sentByType in something other than a std::map, change only this function.
    std::uint64_t sentOfType(const Counters& c, MessageType type)
    {
        const auto it = c.sentPerType.find(type);
        if (it == c.sentPerType.end())
        {
            return 0;
        }
        return it->second;
    }

    // A default Message serializes to 95 signed bytes + 64 bytes of signature.
    constexpr std::uint64_t kDefaultMessageBytes = 159;
}

TEST(Counters, StartAtZero)
{
    const Simulator sim(1, 100, 0);
    const Counters& c = sim.getCounters();

    EXPECT_EQ(c.sent, 0);
    EXPECT_EQ(c.delivered, 0);
    EXPECT_EQ(c.dropped, 0);
    EXPECT_EQ(c.bytesSent, 0);
}

TEST(Counters, CountSentAndDelivered)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(0, 1, 2));
    sim.send(makeMessage(0, 1, 3));
    sim.run(kForever);

    EXPECT_EQ(sim.getCounters().sent, 3);
    EXPECT_EQ(sim.getCounters().delivered, 3);
    EXPECT_EQ(sim.getCounters().dropped, 0);
}

TEST(Counters, DeliveredOnlyAfterRun)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));

    EXPECT_EQ(sim.getCounters().sent, 1);      // counted at send()
    EXPECT_EQ(sim.getCounters().delivered, 0); // not delivered yet

    sim.run(kForever);
    EXPECT_EQ(sim.getCounters().delivered, 1);
}

TEST(Counters, CountDroppedMessages)
{
    Simulator sim(1, 100, 0);
    FixedPolicy dropAll({});
    sim.setNetworkPolicy(dropAll);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(0, 1, 2));
    sim.send(makeMessage(0, 1, 3));
    sim.run(kForever);

    EXPECT_EQ(sim.getCounters().sent, 3);
    EXPECT_EQ(sim.getCounters().delivered, 0);
    EXPECT_EQ(sim.getCounters().dropped, 3);
}

TEST(Counters, DuplicateIsDeliveredTwice)
{
    Simulator sim(1, 100, 0);
    FixedPolicy duplicate({0, 0});
    sim.setNetworkPolicy(duplicate);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    EXPECT_EQ(sim.getCounters().sent, 1);
    EXPECT_EQ(sim.getCounters().delivered, 2);
    EXPECT_EQ(sim.getCounters().dropped, 0);
}

TEST(Counters, MessageToUnknownNodeIsSentButNotDelivered)
{
    Simulator sim(1, 100, 0);

    sim.send(makeMessage(0, 9, 1)); // no node has id 9
    sim.run(kForever);

    EXPECT_EQ(sim.getCounters().sent, 1);
    EXPECT_EQ(sim.getCounters().delivered, 0);
    EXPECT_EQ(sim.getCounters().dropped, 0); // the policy did not drop it
}

TEST(Counters, CountMessagesSentByNodesDuringTheRun)
{
    // Node 1 replies to every message it receives, so 1 message becomes 2.
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node0(0, sim, log);
    TestNode node1(1, sim, log);
    node1.whenMessage = [&sim](const Message& m) { sim.send(makeMessage(1, 0, m.seq + 1)); };
    sim.addNode(0, node0);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    EXPECT_EQ(sim.getCounters().sent, 2);
    EXPECT_EQ(sim.getCounters().delivered, 2);
}

TEST(Counters, TimersAreNotCounted)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 50, 7);
    sim.run(kForever);

    ASSERT_EQ(node1.timerIds.size(), 1); // the timer did fire
    EXPECT_EQ(sim.getCounters().sent, 0);   // but it is not a message
    EXPECT_EQ(sim.getCounters().delivered, 0);
    EXPECT_EQ(sim.getCounters().bytesSent, 0);
}

// ---------- bytes ----------

TEST(Counters, BytesOfOneMessage)
{
    Simulator sim(1, 100, 0);

    sim.send(makeMessage(0, 1, 1));

    EXPECT_EQ(sim.getCounters().bytesSent, kDefaultMessageBytes);
}

TEST(Counters, BytesIncludeTheKeyAndTheValue)
{
    Simulator sim(1, 100, 0);
    Message m = makeMessage(0, 1, 1);
    m.payload.key = "abc";  // 3 more bytes
    m.payload.value = "de"; // 2 more bytes

    sim.send(m);

    EXPECT_EQ(sim.getCounters().bytesSent, kDefaultMessageBytes + 5);
}

TEST(Counters, DroppedMessagesStillCostBytes)
{
    Simulator sim(1, 100, 0);
    FixedPolicy dropAll({});
    sim.setNetworkPolicy(dropAll);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(0, 1, 2));

    EXPECT_EQ(sim.getCounters().bytesSent, 2 * kDefaultMessageBytes);
}

TEST(Counters, DuplicateCostsTheSenderOnlyOnce)
{
    Simulator sim(1, 100, 0);
    FixedPolicy duplicate({0, 0});
    sim.setNetworkPolicy(duplicate);

    sim.send(makeMessage(0, 1, 1));

    EXPECT_EQ(sim.getCounters().bytesSent, kDefaultMessageBytes);
}

// ---------- by message type ----------

TEST(Counters, CountByType)
{
    Simulator sim(1, 100, 0);

    Message prepare = makeMessage(0, 1, 1); // makeMessage gives a PREPARE
    Message commit = makeMessage(0, 1, 2);
    commit.type = MessageType::COMMIT;

    sim.send(prepare);
    sim.send(prepare);
    sim.send(commit);

    const Counters& c = sim.getCounters();
    EXPECT_EQ(sentOfType(c, MessageType::PREPARE), 2);
    EXPECT_EQ(sentOfType(c, MessageType::COMMIT), 1);
    EXPECT_EQ(sentOfType(c, MessageType::PRE_PREPARE), 0);
}