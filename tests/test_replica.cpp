//
// Created by sam on 08/11/2026.
//
#include <gtest/gtest.h>
#include "crypto/fakeSigner.h"
#include "protocol/replica.h"
#include "sim/simulator.h"
#include "test_node.h"

namespace
{
    // A REQUEST from client 100 (ids 0 to n-1 are replicas, so clients use 100 and up).
    Message makeRequest(std::uint64_t requestId, const std::string& key, const std::string& value)
    {
        Message m;
        m.type = MessageType::REQUEST;
        m.sender = 100;
        m.receiver = 0;
        m.payload.clientId = 100;
        m.payload.requestId = requestId;
        m.payload.op = Operation::Put;
        m.payload.key = key;
        m.payload.value = value;
        FakeSigner client(100);
        client.sign(m);
        return m;
    }
}

TEST(RequestDigest, SameRequestSameDigest)
{
    const Message a = makeRequest(1, "x", "1");
    const Message b = makeRequest(1, "x", "1");
    EXPECT_EQ(requestDigest(a.payload), requestDigest(b.payload));
}

TEST(RequestDigest, DifferentRequestDifferentDigest)
{
    EXPECT_NE(requestDigest(makeRequest(1, "x", "1").payload), requestDigest(makeRequest(1, "x", "2").payload));
    EXPECT_NE(requestDigest(makeRequest(1, "x", "1").payload), requestDigest(makeRequest(2, "x", "1").payload));
    EXPECT_NE(requestDigest(makeRequest(1, "ab", "c").payload), requestDigest(makeRequest(1, "a", "bc").payload));
}

TEST(ReplicaPrimary, RequestGivesOnePrePrepareToEachBackup)
{
    Simulator sim(1, 100, 0);
    FakeSigner signer0(0);
    Replica primary(0, sim, 1, signer0);
    Log log;
    TestNode b1(1, sim, log), b2(2, sim, log), b3(3, sim, log);
    sim.addNode(0, primary);
    sim.addNode(1, b1);
    sim.addNode(2, b2);
    sim.addNode(3, b3);

    const Message request = makeRequest(1, "x", "1");
    sim.send(request);
    sim.run(kForever);

    FakeSigner checker(9);
    for (TestNode* backup : {&b1, &b2, &b3})
    {
        ASSERT_EQ(backup->messages.size(), 1);
        const Message& pp = backup->messages[0];
        EXPECT_EQ(pp.type, MessageType::PRE_PREPARE);
        EXPECT_EQ(pp.sender, 0);
        EXPECT_EQ(pp.view, 0);
        EXPECT_EQ(pp.seq, 1); // the first slot is 1
        EXPECT_EQ(pp.payload, request.payload);
        EXPECT_EQ(pp.digest, requestDigest(request.payload));
        EXPECT_TRUE(checker.verify(pp));
    }
    EXPECT_EQ(b1.messages[0].receiver, 1);
    EXPECT_EQ(b2.messages[0].receiver, 2);
    EXPECT_EQ(b3.messages[0].receiver, 3);
}

TEST(ReplicaPrimary, SecondRequestWaitsInTheQueue)
{
    Simulator sim(1, 100, 0);
    FakeSigner signer0(0);
    Replica primary(0, sim, 1, signer0);
    Log log;
    TestNode b1(1, sim, log), b2(2, sim, log), b3(3, sim, log);
    sim.addNode(0, primary);
    sim.addNode(1, b1);
    sim.addNode(2, b2);
    sim.addNode(3, b3);

    sim.send(makeRequest(1, "x", "1"));
    sim.send(makeRequest(2, "y", "2")); // round 0 is still running: it must wait
    sim.run(kForever);

    EXPECT_EQ(b1.messages.size(), 1);
    EXPECT_EQ(b1.messages[0].payload.requestId, 1);
}