//
// Created by sam on 09/11/2026.
//
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "crypto/fakeSigner.h"
#include "protocol/replica.h"
#include "sim/simulator.h"
#include "test_node.h"

namespace
{
    constexpr std::uint32_t kClient = 100; // ids 0 to 3 are replicas

    Request sampleRequest(std::uint64_t requestId, const std::string& value)
    {
        Request r;
        r.clientId = kClient;
        r.requestId = requestId;
        r.op = Operation::Put;
        r.key = "x";
        r.value = value;
        return r;
    }

    // A message about slot `seq`, from `from` to `to`, signed by `from`.
    Message vote(MessageType type, std::uint32_t from, std::uint64_t seq, const Request& request, std::uint32_t to)
    {
        Message m;
        m.type = type;
        m.sender = from;
        m.receiver = to;
        m.view = 0;
        m.seq = seq;
        m.digest = requestDigest(request);
        if (type == MessageType::PRE_PREPARE)
        {
            m.payload = request; // only the PRE-PREPARE carries the request
        }
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

    // Replica 1 is real (a backup). Replicas 0, 2, 3 and the client are TestNodes.
    struct OneBackup
    {
        Simulator sim{1, 100, 0};
        FakeSigner signer1{1};
        Replica backup{1, sim, 1, signer1};
        Log log;
        TestNode r0{0, sim, log};
        TestNode r2{2, sim, log};
        TestNode r3{3, sim, log};
        TestNode client{kClient, sim, log};

        OneBackup()
        {
            sim.addNode(0, r0);
            sim.addNode(1, backup);
            sim.addNode(2, r2);
            sim.addNode(3, r3);
            sim.addNode(kClient, client);
        }

        // PRE-PREPARE from the primary + PREPARE from replica 2: replica 1 is then
        // prepared and sends its own COMMIT (its own vote is 1 of the 3 needed).
        void prepare(std::uint64_t seq, const Request& request)
        {
            sim.send(vote(MessageType::PRE_PREPARE, 0, seq, request, 1));
            sim.send(vote(MessageType::PREPARE, 2, seq, request, 1));
        }

        void commitFrom(std::uint32_t from, std::uint64_t seq, const Request& request)
        {
            sim.send(vote(MessageType::COMMIT, from, seq, request, 1));
        }
    };
}

// ---------- committed or not ----------

TEST(CommitPhase, ThreeCommitsExecuteTheRequest)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(2, 1, request);
    setup.commitFrom(3, 1, request); // own vote + 2 + 3 = 2f + 1
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 1);
    ASSERT_EQ(setup.backup.getLedger().size(), 1);
    EXPECT_EQ(setup.backup.getLedger().at(1), request);
}

TEST(CommitPhase, TwoCommitsAreNotEnough)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(2, 1, request); // own vote + 2 = 2 votes only
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 0);
    EXPECT_TRUE(setup.backup.getLedger().empty());
    EXPECT_TRUE(setup.client.messages.empty());
}

TEST(CommitPhase, ThePrimarysCommitCounts)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(0, 1, request); // the primary sends COMMIT like everyone
    setup.commitFrom(2, 1, request);
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 1);
}

TEST(CommitPhase, CommitFromAClientDoesNotCount)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(kClient, 1, request); // a client is not a replica
    setup.commitFrom(2, 1, request);
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 0);
}

TEST(CommitPhase, CommitForAnotherDigestDoesNotCount)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(2, 1, request);
    setup.commitFrom(3, 1, sampleRequest(1, "2")); // a vote for another request
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 0);
}

TEST(CommitPhase, SameCommitTwiceCountsOnce)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(2, 1, request);
    setup.commitFrom(2, 1, request); // duplicated by the network
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 0);
}

TEST(CommitPhase, CommitsArrivingBeforeItsOwnCommitAreKept)
{
    // The others are faster: their COMMITs arrive before this replica is prepared.
    // Its own COMMIT is then the last vote missing.
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.commitFrom(2, 1, request);
    setup.commitFrom(3, 1, request);
    setup.prepare(1, request);
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 1);
}


// ---------- executing ----------

TEST(CommitPhase, ExecutionChangesTheStore)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "42");

    setup.prepare(1, request);
    setup.commitFrom(2, 1, request);
    setup.commitFrom(3, 1, request);
    setup.sim.run(kForever);

    KVStore expected; // a store that executed the same request on its own
    expected.execute(request);
    EXPECT_EQ(setup.backup.getKVStore().digest(), expected.digest());
}

TEST(CommitPhase, ReplyGoesToTheClient)
{
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    setup.prepare(1, request);
    setup.commitFrom(2, 1, request);
    setup.commitFrom(3, 1, request);
    setup.sim.run(kForever);

    ASSERT_EQ(setup.client.messages.size(), 1);
    const Message& reply = setup.client.messages[0];
    EXPECT_EQ(reply.type, MessageType::REPLY);
    EXPECT_EQ(reply.sender, 1);
    EXPECT_EQ(reply.receiver, kClient);
    EXPECT_EQ(reply.seq, 1);                  // the slot
    EXPECT_EQ(reply.payload.requestId, 1);    // which request it answers
    EXPECT_EQ(reply.result.status, Status::Ok);
    FakeSigner checker(9);
    EXPECT_TRUE(checker.verify(reply));
}

TEST(CommitPhase, SlotsAreExecutedInOrder)
{
    OneBackup setup;
    const Request first = sampleRequest(1, "1");
    const Request second = sampleRequest(2, "2");

    // Slot 2 is fully committed first
    setup.prepare(2, second);
    setup.commitFrom(2, 2, second);
    setup.commitFrom(3, 2, second);
    setup.sim.run(kForever);
    EXPECT_EQ(setup.backup.getLastExecuted(), 0) << "slot 2 must wait for slot 1";

    // then slot 1: both are executed, 1 before 2.
    setup.prepare(1, first);
    setup.commitFrom(2, 1, first);
    setup.commitFrom(3, 1, first);
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 2);
    ASSERT_EQ(setup.backup.getLedger().size(), 2);
    EXPECT_EQ(setup.backup.getLedger().at(1), first);
    EXPECT_EQ(setup.backup.getLedger().at(2), second);
    const std::vector<Message> replies = received(setup.client, MessageType::REPLY);
    ASSERT_EQ(replies.size(), 2);
    EXPECT_EQ(replies[0].seq, 1);
    EXPECT_EQ(replies[1].seq, 2);
}

TEST(CommitPhase, SameRequestInTwoSlotsIsExecutedOnce)
{
    // A lying primary puts the same request in slots 1 and 2.
    OneBackup setup;
    const Request request = sampleRequest(1, "1");

    for (std::uint64_t seq : {1, 2})
    {
        setup.prepare(seq, request);
        setup.commitFrom(2, seq, request);
        setup.commitFrom(3, seq, request);
    }
    setup.sim.run(kForever);

    EXPECT_EQ(setup.backup.getLastExecuted(), 2) << "the duplicate slot is passed, not stuck";
    EXPECT_EQ(setup.backup.getLedger().size(), 1);
    EXPECT_EQ(received(setup.client, MessageType::REPLY).size(), 1);
}



// ---------- the primary moves on ----------

TEST(CommitPhase, PrimaryStartsTheWaitingRequestAfterExecuting)
{
    Simulator sim(1, 100, 0);
    FakeSigner signer0(0);
    Replica primary(0, sim, 1, signer0);
    Log log;
    TestNode r1(1, sim, log);
    TestNode r2(2, sim, log);
    TestNode r3(3, sim, log);
    TestNode client(kClient, sim, log);
    sim.addNode(0, primary);
    sim.addNode(1, r1);
    sim.addNode(2, r2);
    sim.addNode(3, r3);
    sim.addNode(kClient, client);

    // Two requests: the second one waits in the queue.
    const Request first = sampleRequest(1, "1");
    const Request second = sampleRequest(2, "2");
    for (const Request& r : {first, second})
    {
        Message m;
        m.type = MessageType::REQUEST;
        m.sender = kClient;
        m.receiver = 0;
        m.payload = r;
        FakeSigner clientSigner(kClient);
        clientSigner.sign(m);
        sim.send(m);
    }
    sim.run(kForever);
    ASSERT_EQ(received(r1, MessageType::PRE_PREPARE).size(), 1);

    // Replicas 1 and 2 prepare and commit slot 1: the primary executes it
    sim.send(vote(MessageType::PREPARE, 1, 1, first, 0));
    sim.send(vote(MessageType::PREPARE, 2, 1, first, 0));
    sim.send(vote(MessageType::COMMIT, 1, 1, first, 0));
    sim.send(vote(MessageType::COMMIT, 2, 1, first, 0));
    sim.run(kForever);

    // and starts slot 2 with the waiting request.
    EXPECT_EQ(primary.getLastExecuted(), 1);
    const std::vector<Message> prePrepares = received(r1, MessageType::PRE_PREPARE);
    ASSERT_EQ(prePrepares.size(), 2);
    EXPECT_EQ(prePrepares[1].seq, 2);
    EXPECT_EQ(prePrepares[1].payload, second);
}