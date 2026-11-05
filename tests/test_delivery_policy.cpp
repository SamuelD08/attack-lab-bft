//
// Created by sam on 05/11/2026.
//
// Tests for the delivery hook (DeliveryPolicy, Simulator::setNetworkPolicy)
// and for the random policy (SimPolicy).
//
// The rules these tests check:
//  - decide() returns one extra delay per copy: {} = drop, {0} = normal,
//    {500} = 500 µs late, {0, 0} = duplicate.
//  - Each extra delay is counted from the normal arrival time
//    (now + baseDelay + random extra), not from the previous copy.
//  - The policy is asked exactly once per send(), with the message and the
//    current time. Timers never go through the policy.
//  - Dropping a message does not change when the other messages arrive.
// Simple tests for the delivery hook and for SimPolicy.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "sim/deliveryPolicy.h"
#include "sim/simPolicy.h"
#include "sim/simulator.h"
#include "test_node.h"

namespace
{
    // Always gives the same answer, and counts how many times it was asked.
    class Always : public DeliveryPolicy
    {
    private:
        std::vector<std::uint64_t> delays;

    public:
        int calls = 0;

        explicit Always(std::vector<std::uint64_t> answer) : delays(answer) {}

        std::vector<std::uint64_t> decide(const Message&, std::uint64_t) override
        {
            ++calls;
            return delays;
        }
    };

    // Drops every message sent by one node, delivers the others normally.
    class DropFrom : public DeliveryPolicy
    {
    private:
        std::uint32_t silentNode;

    public:
        explicit DropFrom(std::uint32_t node) : silentNode(node) {}

        std::vector<std::uint64_t> decide(const Message& message, std::uint64_t) override
        {
            if (message.sender == silentNode)
            {
                return {};
            }
            return {0};
        }
    };
}

// ---------- the hook ----------

TEST(DeliveryPolicy, DropAllDeliversNothing)
{
    Simulator sim(1, 100, 0);
    Always dropAll({});
    sim.setNetworkPolicy(dropAll);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(0, 1, 2));
    sim.run(kForever);

    EXPECT_TRUE(node1.messages.empty());
}

TEST(DeliveryPolicy, DropOnlyMessagesFromNode2)
{
    Simulator sim(1, 100, 0);
    DropFrom dropNode2(2);
    sim.setNetworkPolicy(dropNode2);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(2, 1, 2)); // dropped
    sim.send(makeMessage(3, 1, 3));
    sim.run(kForever);

    ASSERT_EQ(node1.messages.size(), 2);
    EXPECT_EQ(node1.messages[0].seq, 1);
    EXPECT_EQ(node1.messages[1].seq, 3);
}

TEST(DeliveryPolicy, AddedDelayMakesTheMessageLate)
{
    Simulator sim(1, 100, 0);
    Always late({500});
    sim.setNetworkPolicy(late);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    ASSERT_EQ(node1.messageTimes.size(), 1u);
    EXPECT_EQ(node1.messageTimes[0], 600); // 100 + 500
}

TEST(DeliveryPolicy, DuplicateDeliversTwoCopies)
{
    Simulator sim(1, 100, 0);
    Always duplicate({0, 0});
    sim.setNetworkPolicy(duplicate);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    const Message sent = makeMessage(0, 1, 1);
    sim.send(sent);
    sim.run(kForever);

    ASSERT_EQ(node1.messages.size(), 2);
    EXPECT_EQ(node1.messages[0], sent);
    EXPECT_EQ(node1.messages[1], sent);
}

TEST(DeliveryPolicy, EachCopyHasItsOwnDelay)
{
    // {300, 0}: the second copy must NOT keep the first copy's 300 µs.
    Simulator sim(1, 100, 0);
    Always policy({300, 0});
    sim.setNetworkPolicy(policy);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    ASSERT_EQ(node1.messageTimes.size(), 2);
    EXPECT_EQ(node1.messageTimes[0], 100); // copy with delay 0 (delivered first)
    EXPECT_EQ(node1.messageTimes[1], 400); // copy with delay 300
}

TEST(DeliveryPolicy, PolicyIsAskedOncePerMessage)
{
    Simulator sim(1, 100, 0);
    Always duplicate({0, 0});
    sim.setNetworkPolicy(duplicate);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(0, 1, 2));
    sim.send(makeMessage(0, 1, 3));
    sim.run(kForever);

    EXPECT_EQ(duplicate.calls, 3);
    EXPECT_EQ(node1.messages.size(), 6);
}

TEST(DeliveryPolicy, TimersIgnoreThePolicy)
{
    Simulator sim(1, 100, 0);
    Always dropAll({});
    sim.setNetworkPolicy(dropAll);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 50, 7);
    sim.run(kForever);

    ASSERT_EQ(node1.timerIds.size(), 1);
    EXPECT_EQ(node1.timerIds[0], 7);
    EXPECT_EQ(dropAll.calls, 0); // a timer is not a network message
}

TEST(DeliveryPolicy, DroppingAMessageDoesNotChangeTheOthers)
{
    // Two runs with the same seed and random delays.
    // Run A: no policy. Run B: messages from node 2 are dropped.
    Simulator simA(7, 100, 1000);
    Log logA;
    TestNode nodeA(1, simA, logA);
    simA.addNode(1, nodeA);

    Simulator simB(7, 100, 1000);
    DropFrom dropNode2(2);
    simB.setNetworkPolicy(dropNode2);
    Log logB;
    TestNode nodeB(1, simB, logB);
    simB.addNode(1, nodeB);

    for (Simulator* sim : {&simA, &simB})
    {
        sim->send(makeMessage(0, 1, 1));
        sim->send(makeMessage(2, 1, 2)); // dropped in run B
        sim->send(makeMessage(0, 1, 3));
        sim->run(kForever);
    }

    // Run B must be run A without the line of message 2: same times for the others.
    std::erase_if(logA, [](const std::string& line) { return line.find("seq=2 ") != std::string::npos; });
    EXPECT_EQ(logB, logA);
}

// ---------- SimPolicy ----------

TEST(SimPolicy, SameSeedGivesTheSameDecisions)
{
    SimPolicy a(42);
    SimPolicy b(42);
    const Message m = makeMessage(0, 1, 1);

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(a.decide(m, 0), b.decide(m, 0));
    }
}

TEST(SimPolicy, NeverMoreThanTwoCopies)
{
    SimPolicy policy(42);
    const Message m = makeMessage(0, 1, 1);

    for (int i = 0; i < 1000; ++i)
    {
        EXPECT_LE(policy.decide(m, 0).size(), 2);
    }
}

TEST(SimPolicy, SometimesDuplicatesWithADelay)
{
    // In 1000 decisions, DuplicatedWithDelay ({0, d} with d > 0) must happen at least once.
    SimPolicy policy(42);
    const Message m = makeMessage(0, 1, 1);
    bool found = false;

    for (int i = 0; i < 1000; ++i)
    {
        const std::vector<std::uint64_t> d = policy.decide(m, 0);
        if (d.size() == 2 && d[1] > 0)
        {
            found = true;
        }
    }
    EXPECT_TRUE(found) << "DuplicatedWithDelay never happened: check the range of the status";
}