//
// Created by sam on 05/11/2026.
//
// Tests for the discrete-event simulator (Simulator, Transport, Node).
//
// The rules these tests check:
//  - A message arrives at now() + baseDelay + extra, where extra is random in
//    [0, maxExtraDelay] (both ends included). All times are in microseconds.
//  - A timer fires at exactly now() + delay (no random part).
//  - Events come out in time order. Events at the same time come out in the
//    order they were scheduled.
//  - run(timeLimit) handles every event whose time is <= timeLimit (an absolute
//    time) and keeps the later ones for the next run().
//  - A message or timer for an unknown node id is dropped, without crashing.
//  - The same seed always gives exactly the same run.
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "sim/simulator.h"

namespace
{
    constexpr std::uint64_t kForever = std::numeric_limits<std::uint64_t>::max();

    // One line per delivery, shared by all the nodes of a test, in delivery order.
    using Log = std::vector<std::string>;

    Message makeMessage(std::uint32_t from, std::uint32_t to, std::uint64_t seq)
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

    // Sends `count` messages to one node at time 0 and returns their arrival
    // times, in the order they were delivered.
    std::vector<std::uint64_t> arrivalTimes(std::uint64_t seed, std::uint64_t baseDelay,
                                            std::uint64_t maxExtraDelay, std::uint64_t count)
    {
        Simulator sim(seed, baseDelay, maxExtraDelay);
        Log log;
        TestNode node1(1, sim, log);
        sim.addNode(1, node1);

        for (std::uint64_t seq = 0; seq < count; ++seq)
        {
            sim.send(makeMessage(0, 1, seq));
        }
        sim.run(kForever);
        return node1.messageTimes;
    }

    // Four nodes pass messages around a ring: each message is forwarded to the
    // next node until its seq reaches `rounds`. Four chains start at the same
    // time, so the random delays change how they mix. Returns the whole log.
    Log runRing(std::uint64_t seed, std::uint64_t rounds)
    {
        Simulator sim(seed, 100, 1000);
        Log log;
        TestNode n0(0, sim, log);
        TestNode n1(1, sim, log);
        TestNode n2(2, sim, log);
        TestNode n3(3, sim, log);

        for (TestNode* node : {&n0, &n1, &n2, &n3})
        {
            node->whenMessage = [&sim, rounds](const Message& m)
            {
                if (m.seq < rounds)
                {
                    sim.send(makeMessage(m.receiver, (m.receiver + 1) % 4, m.seq + 1));
                }
            };
        }
        sim.addNode(0, n0);
        sim.addNode(1, n1);
        sim.addNode(2, n2);
        sim.addNode(3, n3);

        for (std::uint32_t i = 0; i < 4; ++i)
        {
            sim.send(makeMessage(i, (i + 1) % 4, 1));
        }
        sim.run(kForever);
        return log;
    }
}

// ---------- basics ----------

TEST(SimulatorBasics, ClockStartsAtZero)
{
    const Simulator sim(1, 100, 0); // const: now() must be a const function
    EXPECT_EQ(sim.now(), 0);
}

TEST(SimulatorBasics, RunWithNothingToDoIsFine)
{
    Simulator sim(1, 100, 0);
    sim.run(kForever); // must not crash or hang
}

TEST(SimulatorBasics, SendOnlySchedules)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    EXPECT_TRUE(node1.messages.empty()) << "delivered before run()";

    sim.run(kForever);
    EXPECT_EQ(node1.messages.size(), 1);
}

TEST(SimulatorBasics, DeliversOnlyToTheReceiver)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node0(0, sim, log);
    TestNode node1(1, sim, log);
    TestNode node2(2, sim, log);
    sim.addNode(0, node0);
    sim.addNode(1, node1);
    sim.addNode(2, node2);

    sim.send(makeMessage(0, 2, 1));
    sim.run(kForever);

    EXPECT_TRUE(node0.messages.empty());
    EXPECT_TRUE(node1.messages.empty());
    EXPECT_EQ(node2.messages.size(), 1);
}

TEST(SimulatorBasics, MessageArrivesUnchanged)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    Message sent = makeMessage(0, 1, 42);
    sent.type = MessageType::COMMIT;
    sent.view = 3;
    sent.digest.fill(0xAB);
    sent.payload.clientId = 7;
    sent.payload.requestId = 1000;
    sent.payload.op = Operation::Put;
    sent.payload.key = "key";
    sent.payload.value = "value";
    sent.result.status = Status::Ok;
    sent.result.value = "result";
    sent.signature.fill(0x5A);

    sim.send(sent);
    sim.run(kForever);

    ASSERT_EQ(node1.messages.size(), 1);
    EXPECT_EQ(node1.messages[0], sent);
}

TEST(SimulatorBasics, ArrivesAfterExactlyTheBaseDelay)
{
    Simulator sim(1, 100, 0); // no extra delay
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    ASSERT_EQ(node1.messageTimes.size(), 1);
    EXPECT_EQ(node1.messageTimes[0], 100) << "now() during delivery must be the arrival time";
}

TEST(SimulatorBasics, MessageToUnknownNodeIsDropped)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 9, 1)); // no node has id 9
    sim.send(makeMessage(0, 1, 2));
    sim.run(kForever);

    ASSERT_EQ(node1.messages.size(), 1);
    EXPECT_EQ(node1.messages[0].seq, 2);
}

TEST(SimulatorBasics, WorksThroughTheTransportInterface)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    Transport& transport = sim; // this is how the replicas will see it
    transport.send(makeMessage(0, 1, 1));
    transport.setTimer(1, 300, 7);
    sim.run(kForever);

    EXPECT_EQ(log, (Log{"t=100 node=1 message seq=1 from=0", "t=300 node=1 timer=7"}));
}

// ---------- delays ----------

TEST(SimulatorDelays, StayWithinTheBounds)
{
    const std::vector<std::uint64_t> times = arrivalTimes(42, 100, 50, 1000);
    ASSERT_EQ(times.size(), 1000);

    const auto [earliest, latest] = std::minmax_element(times.begin(), times.end());
    EXPECT_GE(*earliest, 100);
    EXPECT_LE(*latest, 150);
}

TEST(SimulatorDelays, UseTheWholeRange)
{
    // With 1000 messages and only 51 possible delays, every delay shows up.
    const std::vector<std::uint64_t> times = arrivalTimes(42, 100, 50, 1000);
    ASSERT_EQ(times.size(), 1000);

    const auto [earliest, latest] = std::minmax_element(times.begin(), times.end());
    EXPECT_EQ(*earliest, 100) << "an extra delay of 0 never happened";
    EXPECT_EQ(*latest, 150) << "the maximum extra delay never happened";
}

TEST(SimulatorDelays, AreCountedFromTheCurrentTime)
{
    // A message bounces between two nodes; each hop takes exactly 100.
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node0(0, sim, log);
    TestNode node1(1, sim, log);
    const auto replyToSender = [&sim](const Message& m)
    {
        if (m.seq < 4)
        {
            sim.send(makeMessage(m.receiver, m.sender, m.seq + 1));
        }
    };
    node0.whenMessage = replyToSender;
    node1.whenMessage = replyToSender;
    sim.addNode(0, node0);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1));
    sim.run(kForever);

    EXPECT_EQ(log, (Log{
                       "t=100 node=1 message seq=1 from=0",
                       "t=200 node=0 message seq=2 from=1",
                       "t=300 node=1 message seq=3 from=0",
                       "t=400 node=0 message seq=4 from=1",
                   }));
}

TEST(SimulatorDelays, TimeNeverGoesBackwards)
{
    const std::vector<std::uint64_t> times = arrivalTimes(7, 100, 1000, 500);
    ASSERT_EQ(times.size(), 500);
    EXPECT_TRUE(std::is_sorted(times.begin(), times.end())) << "events came out of time order";
}

// ---------- order of events ----------

TEST(SimulatorOrder, SameTimeMessagesKeepTheirSendingOrder)
{
    Simulator sim(1, 100, 0); // every message arrives at t=100
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    for (std::uint64_t seq = 1; seq <= 10; ++seq)
    {
        sim.send(makeMessage(0, 1, seq));
    }
    sim.run(kForever);

    std::vector<std::uint64_t> seqs;
    for (const Message& m : node1.messages)
    {
        seqs.push_back(m.seq);
    }
    EXPECT_EQ(seqs, (std::vector<std::uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
}

TEST(SimulatorOrder, SameTimeTimersKeepTheirSettingOrder)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    for (std::uint64_t timerId = 1; timerId <= 10; ++timerId)
    {
        sim.setTimer(1, 50, timerId);
    }
    sim.run(kForever);

    EXPECT_EQ(node1.timerIds, (std::vector<std::uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}));
}

TEST(SimulatorOrder, EarlierEventComesFirstEvenIfScheduledLater)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1)); // scheduled first, arrives at t=100
    sim.setTimer(1, 50, 7);         // scheduled second, fires at t=50
    sim.run(kForever);

    EXPECT_EQ(log, (Log{"t=50 node=1 timer=7", "t=100 node=1 message seq=1 from=0"}));
}

// ---------- time limit ----------

TEST(SimulatorTimeLimit, StopsAtTheLimit)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 100, 1);
    sim.setTimer(1, 300, 2);
    sim.run(200);

    EXPECT_EQ(node1.timerIds, (std::vector<std::uint64_t>{1}));
}

TEST(SimulatorTimeLimit, IncludesAnEventExactlyAtTheLimit)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 200, 1);
    sim.run(200);

    EXPECT_EQ(node1.timerIds, (std::vector<std::uint64_t>{1}));
}

TEST(SimulatorTimeLimit, KeepsLaterEventsForTheNextRun)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 100, 1);
    sim.setTimer(1, 300, 2);
    sim.run(200);
    sim.run(400);

    EXPECT_EQ(log, (Log{"t=100 node=1 timer=1", "t=300 node=1 timer=2"}));
}

// ---------- timers ----------

TEST(SimulatorTimers, FireAfterExactlyTheirDelay)
{
    Simulator sim(1, 100, 1000); // random message delays must not affect timers
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(1, 500, 7);
    sim.run(kForever);

    EXPECT_EQ(log, (Log{"t=500 node=1 timer=7"}));
}

TEST(SimulatorTimers, GoOnlyToTheirNode)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    TestNode node2(2, sim, log);
    sim.addNode(1, node1);
    sim.addNode(2, node2);

    sim.setTimer(2, 50, 7);
    sim.run(kForever);

    EXPECT_TRUE(node1.timerIds.empty());
    EXPECT_EQ(node2.timerIds, (std::vector<std::uint64_t>{7}));
}

TEST(SimulatorTimers, AreCountedFromTheCurrentTime)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    node1.whenMessage = [&sim](const Message&) { sim.setTimer(1, 50, 7); };
    sim.addNode(1, node1);

    sim.send(makeMessage(0, 1, 1)); // arrives at t=100, then sets a 50 µs timer
    sim.run(kForever);

    EXPECT_EQ(log, (Log{"t=100 node=1 message seq=1 from=0", "t=150 node=1 timer=7"}));
}

TEST(SimulatorTimers, TimerCanStartTheNextTimer)
{
    // Like a view-change timer that is started again each time it fires.
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    node1.whenTimer = [&sim](std::uint64_t timerId)
    {
        if (timerId < 3)
        {
            sim.setTimer(1, 100, timerId + 1);
        }
    };
    sim.addNode(1, node1);

    sim.setTimer(1, 100, 1);
    sim.run(kForever);

    EXPECT_EQ(log, (Log{"t=100 node=1 timer=1", "t=200 node=1 timer=2", "t=300 node=1 timer=3"}));
}

TEST(SimulatorTimers, TimerForUnknownNodeIsIgnored)
{
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    sim.addNode(1, node1);

    sim.setTimer(9, 50, 7); // no node has id 9
    sim.setTimer(1, 60, 8);
    sim.run(kForever);

    EXPECT_EQ(log, (Log{"t=60 node=1 timer=8"}));
}

// ---------- a node calls the simulator while it is being called ----------

TEST(SimulatorReentrancy, NodeCanSendManyMessagesWhileReceiving)
{
    // Node 1 sends 100 messages from inside onMessage, and only then reads the
    // message it received. If run() still holds a reference into the queue, that
    // message may already be freed. Run with -fsanitize=address to catch it.
    Simulator sim(1, 100, 0);
    Log log;
    TestNode node1(1, sim, log);
    TestNode node2(2, sim, log);
    std::uint64_t seqReadAfterSending = 0;
    node1.whenMessage = [&sim, &seqReadAfterSending](const Message& m)
    {
        for (std::uint64_t i = 0; i < 100; ++i)
        {
            sim.send(makeMessage(1, 2, i));
        }
        seqReadAfterSending = m.seq;
    };
    sim.addNode(1, node1);
    sim.addNode(2, node2);

    sim.send(makeMessage(0, 1, 77));
    sim.run(kForever);

    EXPECT_EQ(seqReadAfterSending, 77);
    EXPECT_EQ(node2.messages.size(), 100);
}

// ---------- determinism ----------

TEST(SimulatorDeterminism, SameSeedGivesTheSameRun)
{
    const Log first = runRing(42, 20);
    const Log second = runRing(42, 20);

    ASSERT_EQ(first.size(), 80); // 4 chains of 20 messages
    EXPECT_EQ(first, second);
}

TEST(SimulatorDeterminism, DifferentSeedsGiveDifferentRuns)
{
    EXPECT_NE(runRing(42, 20), runRing(43, 20));
}