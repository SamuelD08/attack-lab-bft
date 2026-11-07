//
// Created by sam on 07/11/2026.
//
// Simple tests for the trace.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "sim/deliveryPolicy.h"
#include "sim/simulator.h"
#include "sim/trace.h"
#include "test_node.h"

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

    // Just the kinds of the entries in order: easy to compare.
    std::vector<TraceKind> kindsOf(const std::vector<Record>& trace)
    {
        std::vector<TraceKind> kinds;
        for (const Record& entry : trace)
        {
            kinds.push_back(entry.kind);
        }
        return kinds;
    }
}

TEST(Trace, EmptyAtStart)
{
    const Simulator sim(1, 100, 0);
    EXPECT_TRUE(sim.getTrace().empty());
}

TEST(Trace, MessageIsSentThenDelivered)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 5));
    sim.run(kForever);

    const std::vector<Record>& trace = sim.getTrace();
    ASSERT_EQ(kindsOf(trace), (std::vector<TraceKind>{TraceKind::Sent, TraceKind::Delivered}));

    EXPECT_EQ(trace[0].time, 0);   // sent at t=0
    EXPECT_EQ(trace[1].time, 100); // delivered at t=100
    EXPECT_EQ(trace[0].messageId, trace[1].messageId);

    EXPECT_EQ(trace[0].sender, 0);
    EXPECT_EQ(trace[0].receiver, 1);
    EXPECT_EQ(trace[0].type, MessageType::PREPARE);
    EXPECT_EQ(trace[0].seq, 5);
}

TEST(Trace, DroppedMessage)
{
    Simulator sim(1, 100, 0);
    FixedPolicy dropAll({});
    sim.setNetworkPolicy(dropAll);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    const std::vector<Record>& trace = sim.getTrace();
    ASSERT_EQ(kindsOf(trace), (std::vector<TraceKind>{TraceKind::Sent, TraceKind::Dropped}));
    EXPECT_EQ(trace[1].time, 0); // dropped when it was sent
    EXPECT_EQ(trace[0].messageId, trace[1].messageId);
}

TEST(Trace, DuplicateIsDeliveredTwiceWithTheSameId)
{
    Simulator sim(1, 100, 0);
    FixedPolicy duplicate({0, 0});
    sim.setNetworkPolicy(duplicate);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    const std::vector<Record>& trace = sim.getTrace();
    ASSERT_EQ(kindsOf(trace),
              (std::vector<TraceKind>{TraceKind::Sent, TraceKind::Delivered, TraceKind::Delivered}));
    EXPECT_EQ(trace[1].messageId, trace[0].messageId);
    EXPECT_EQ(trace[2].messageId, trace[0].messageId);
}

TEST(Trace, EachSendGetsItsOwnId)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.send(makeMessage(0, 1, 2));

    const std::vector<Record>& trace = sim.getTrace();
    ASSERT_EQ(trace.size(), 2);
    EXPECT_NE(trace[0].messageId, trace[1].messageId);
}

TEST(Trace, TimerFired)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 50, 7);
    EXPECT_TRUE(sim.getTrace().empty()); // setting a timer is not traced

    sim.run(kForever);

    const std::vector<Record>& trace = sim.getTrace(); // only firing it
    ASSERT_EQ(kindsOf(trace), (std::vector<TraceKind>{TraceKind::TimerFired}));
    EXPECT_EQ(trace[0].time, 50);
    EXPECT_EQ(trace[0].receiver, 1);
    EXPECT_EQ(trace[0].timerId, 7);
}

TEST(Trace, MessageToUnknownNodeIsUndeliverable)
{
    Simulator sim(1, 100, 0);

    sim.send(makeMessage(0, 9, 1)); // no node has id 9
    sim.run(kForever);

    sim.printTrace(std::cout);

    EXPECT_EQ(kindsOf(sim.getTrace()), (std::vector<TraceKind>{TraceKind::Sent, TraceKind::Undeliverable}));
}

TEST(Trace, ReactionComesAfterTheDelivery)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node0(0, sim, log);
    TestNode node1(1, sim, log);
    node1.whenMessage = [&sim](const Message& m) { sim.send(makeMessage(1, 0, m.seq + 1)); };
    sim.addNode(0, node0);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    EXPECT_EQ(kindsOf(sim.getTrace()),
              (std::vector<TraceKind>{TraceKind::Sent, TraceKind::Delivered, TraceKind::Sent, TraceKind::Delivered}));
}

TEST(Trace, TimesNeverGoDown)
{
    Simulator sim(7, 100, 1000); // random delays
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);
    sim.setTimer(1, 500, 1);

    for (std::uint64_t seq = 0; seq < 50; ++seq)
    {
        sim.send(makeMessage(0, 1, seq));
    }
    sim.run(kForever);

    const std::vector<Record>& trace = sim.getTrace();
    ASSERT_EQ(trace.size(), 101); // 50 sent + 50 delivered + 1 timer
    EXPECT_TRUE(std::is_sorted(trace.begin(), trace.end(),
                               [](const Record& a, const Record& b) { return a.time < b.time; }));
}

TEST(Trace, DisabledTraceStaysEmptyButCountersStillCount)
{
    Simulator sim(1, 100, 0);
    sim.setTraceEnabled(false);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    EXPECT_TRUE(sim.getTrace().empty());
    EXPECT_EQ(sim.getCounters().delivered, 1);
}

TEST(Trace, ToStringShowsTheMainFields)
{
    Record record;
    record.time = 118;
    record.kind = TraceKind::Delivered;
    record.messageId = 3;
    record.sender = 0;
    record.receiver = 2;
    record.type = MessageType::PRE_PREPARE;
    record.seq = 1;

    const std::string line = toString(record);

    for (const char* part : {"t=118", "msg#3", "DELIVERED", "PRE_PREPARE", "seq=1", "0 -> 2"})
    {
        EXPECT_NE(line.find(part), std::string::npos) << "missing \"" << part << "\" in: " << line;
    }
}