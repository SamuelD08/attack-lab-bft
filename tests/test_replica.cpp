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

    Request sampleRequest(std::uint64_t requestId, const std::string& value)
    {
        Request r;
        r.clientId = 100; // ids 0 to 3 are replicas, so clients use 100 and up
        r.requestId = requestId;
        r.op = Operation::Put;
        r.key = "x";
        r.value = value;
        return r;
    }

    Message prePrepare(std::uint64_t seq, const Request& request, std::uint32_t from = 0, std::uint64_t view = 0)
    {
        Message m;
        m.type = MessageType::PRE_PREPARE;
        m.sender = from;
        m.receiver = 1;
        m.view = view;
        m.seq = seq;
        m.payload = request;
        m.digest = requestDigest(request);
        FakeSigner signer(from);
        signer.sign(m);
        return m;
    }

    std::vector<Message> received(const TestNode& node, MessageType type)
    {
        std::vector<Message> result;
        for (const Message& m : node.messages)
        {
            if (m.type == type)
            {
                result.push_back(m);
            }
        }
        return result;
    }

    // Replica 1 is real (a backup). Replicas 0, 2 and 3 are TestNodes.
    struct OneBackup
    {
        Simulator sim{1, 100, 0};
        FakeSigner signer1{1};
        Replica backup{1, sim, 1, signer1};
        Log log;
        TestNode r0{0, sim, log};
        TestNode r2{2, sim, log};
        TestNode r3{3, sim, log};

        OneBackup()
        {
            sim.addNode(0, r0);
            sim.addNode(1, backup);
            sim.addNode(2, r2);
            sim.addNode(3, r3);
        }

        // How many messages of this type replica 1 sent to the three others, in total.
        [[nodiscard]] std::size_t sentByBackup(const MessageType type) const
        {
            return received(r0, type).size() + received(r2, type).size() + received(r3, type).size();
        }
    };
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


// ---------- (b) a backup receives a PRE-PREPARE ----------

TEST(PrePreparePhase, ValidPrePrepareGivesOnePrepareToEachOtherReplica)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.sim.send(prePrepare(1, request));
    setup.sim.run(kForever);

    FakeSigner checker(9);
    for (const TestNode* node : {&setup.r0, &setup.r2, &setup.r3})
    {
        const std::vector<Message> prepares = received(*node, MessageType::PREPARE);
        ASSERT_EQ(prepares.size(), 1);
        EXPECT_EQ(prepares[0].sender, 1);
        EXPECT_EQ(prepares[0].view, 0);
        EXPECT_EQ(prepares[0].seq, 1);
        EXPECT_EQ(prepares[0].digest, requestDigest(request));
        EXPECT_TRUE(checker.verify(prepares[0]));
    }
    EXPECT_EQ(received(setup.r0, MessageType::PREPARE)[0].receiver, 0); // the primary gets one too
}

TEST(PrePreparePhase, UnsignedPrePrepareIsIgnored)
{
    OneBackup setup;
    Message m = prePrepare(1, sampleRequest(1, "1"));
    m.signature.fill(0);

    setup.sim.send(m);
    setup.sim.run(kForever);

    EXPECT_EQ(setup.sentByBackup(MessageType::PREPARE), 0);
}

TEST(PrePreparePhase, PrePrepareNotFromThePrimaryIsIgnored)
{
    OneBackup setup;

    setup.sim.send(prePrepare(1, sampleRequest(1, "1"), 2)); // replica 2 is not the primary
    setup.sim.run(kForever);

    EXPECT_EQ(setup.sentByBackup(MessageType::PREPARE), 0);
}

TEST(PrePreparePhase, PrePrepareForAnotherViewIsIgnored)
{
    OneBackup setup;

    setup.sim.send(prePrepare(1, sampleRequest(1, "1"), 0, 1));
    setup.sim.run(kForever);

    EXPECT_EQ(setup.sentByBackup(MessageType::PREPARE), 0);
}

TEST(PrePreparePhase, PrePrepareWithAWrongDigestIsIgnored)
{
    OneBackup setup;
    Message m = prePrepare(1, sampleRequest(1, "1"));
    m.digest = requestDigest(sampleRequest(1, "2")); // the digest of another request
    FakeSigner primary(0);
    primary.sign(m);

    setup.sim.send(m);
    setup.sim.run(kForever);

    EXPECT_EQ(setup.sentByBackup(MessageType::PREPARE), 0);
}

TEST(PrePreparePhase, SecondRequestForTheSameSlotIsIgnored)
{
    OneBackup setup;
    const Request first = sampleRequest(1, "1");

    setup.sim.send(prePrepare(1, first));
    setup.sim.send(prePrepare(1, sampleRequest(2, "2"))); // a lying primary: another request, same slot
    setup.sim.run(kForever);

    const std::vector<Message> prepares = received(setup.r2, MessageType::PREPARE);
    ASSERT_EQ(prepares.size(), 1);
    EXPECT_EQ(prepares[0].digest, requestDigest(first));
}

TEST(PrePreparePhase, DuplicatedPrePrepareGivesPreparesOnlyOnce)
{
    OneBackup setup;
    const Message m = prePrepare(1, sampleRequest(1, "1"));

    setup.sim.send(m);
    setup.sim.send(m); // the network duplicated it
    setup.sim.run(kForever);

    EXPECT_EQ(setup.sentByBackup(MessageType::PREPARE), 3);
}