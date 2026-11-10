//
// Created by sam on 09/11/2026.
//
#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "crypto/fakeSigner.h"
#include "protocol/replica.h"
#include "sim/simulator.h"
#include "test_node.h"

namespace
{
    // How many messages of one type were sent. If your Counters struct uses
    // another name than sentByType, change only this function.
    std::uint64_t sentOfType(const Counters& c, MessageType type)
    {
        const auto it = c.sentPerType.find(type);
        return it == c.sentPerType.end() ? 0 : it->second;
    }

    Message clientRequest(std::uint32_t client, std::uint64_t requestId)
    {
        Message m;
        m.type = MessageType::REQUEST;
        m.sender = client;
        m.receiver = 0; // the primary in view 0
        m.payload.clientId = client;
        m.payload.requestId = requestId;
        m.payload.op = Operation::Put;
        m.payload.key = "key" + std::to_string(client);
        m.payload.value = std::to_string(requestId);
        FakeSigner signer(client);
        signer.sign(m);
        return m;
    }

    // Four real replicas (f = 1) and two clients, 100 and 101.
    struct System
    {
        Simulator sim;
        FakeSigner s0{0}, s1{1}, s2{2}, s3{3};
        Replica r0{0, sim, 1, s0};
        Replica r1{1, sim, 1, s1};
        Replica r2{2, sim, 1, s2};
        Replica r3{3, sim, 1, s3};
        Log log;
        TestNode c100{100, sim, log};
        TestNode c101{101, sim, log};

        std::map<std::uint32_t, std::uint64_t> current; // client -> request it is waiting for
        std::map<std::uint32_t, int> replies;           // client -> replies received for it

        System(std::uint64_t seed, std::uint64_t maxExtraDelay) : sim(seed, 100, maxExtraDelay)
        {
            sim.addNode(0, r0);
            sim.addNode(1, r1);
            sim.addNode(2, r2);
            sim.addNode(3, r3);
            sim.addNode(100, c100);
            sim.addNode(101, c101);
        }

        std::vector<const Replica*> replicas() const { return {&r0, &r1, &r2, &r3}; }

        // Each client sends `perClient` requests, one at a time.
        void runWorkload(std::uint64_t perClient)
        {
            for (TestNode* client : {&c100, &c101})
            {
                client->whenMessage = [this, perClient](const Message& m)
                {
                    const std::uint32_t me = m.receiver;
                    if (m.type != MessageType::REPLY || m.payload.requestId != current[me])
                    {
                        return; // a late reply to an older request
                    }
                    if (++replies[me] == 2 && current[me] < perClient) // f + 1 matching replies
                    {
                        replies[me] = 0;
                        ++current[me];
                        sim.send(clientRequest(me, current[me]));
                    }
                };
            }
            current = {{100, 1}, {101, 1}};
            sim.send(clientRequest(100, 1));
            sim.send(clientRequest(101, 1));
            sim.run(kForever);
        }
    };
}

TEST(PbftRound, OneRequestCostsTwentyFourProtocolMessages)
{
    System system(1, 0);
    system.sim.send(clientRequest(100, 1));
    system.sim.run(kForever);

    const Counters c = system.sim.getCounters();
    EXPECT_EQ(sentOfType(c, MessageType::PRE_PREPARE), 3);  // primary -> 3 backups
    EXPECT_EQ(sentOfType(c, MessageType::PREPARE), 9);      // 3 backups -> 3 others
    EXPECT_EQ(sentOfType(c, MessageType::COMMIT), 12);      // 4 replicas -> 3 others
    EXPECT_EQ(sentOfType(c, MessageType::REPLY), 4);        // 4 replicas -> client
}

TEST(PbftRound, OneRequestGetsFourMatchingReplies)
{
    System system(1, 0);
    system.sim.send(clientRequest(100, 1));
    system.sim.run(kForever);

    ASSERT_EQ(system.c100.messages.size(), 4);
    for (const Message& reply : system.c100.messages)
    {
        EXPECT_EQ(reply.type, MessageType::REPLY);
        EXPECT_EQ(reply.seq, 1);
        EXPECT_EQ(reply.payload.requestId, 1);
        EXPECT_EQ(reply.result, system.c100.messages[0].result); // all replicas agree
    }
}

TEST(PbftRound, HundredRequestsWithoutDelaysAllFinish)
{
    System system(1, 0);
    system.runWorkload(50);

    for (const Replica* r : system.replicas())
    {
        EXPECT_EQ(r->getLastExecuted(), 100);
        EXPECT_EQ(r->getLedger().size(), 100); // one entry per slot
        EXPECT_EQ(r->getLedger(), system.r0.getLedger());
    }
}

TEST(PbftRound, HundredRequestsWithRandomDelaysAllFinish)
{
    // Delays from 100 to 1100 µs: messages overtake each other all the time.
    System system(7, 1000);
    system.runWorkload(50);

    for (const Replica* r : system.replicas())
    {
        EXPECT_EQ(r->getLastExecuted(), 100);
        EXPECT_EQ(r->getLedger().size(), 100);                                    // one entry per slot
        EXPECT_EQ(r->getLedger(), system.r0.getLedger());                          // same order
        EXPECT_EQ(r->getKVStore().digest(), system.r0.getKVStore().digest()); // same state
    }
    EXPECT_EQ(system.current[100], 50);
    EXPECT_EQ(system.current[101], 50);
}

TEST(PbftRound, SameSeedGivesTheSameRun)
{
    System first(7, 1000);
    first.runWorkload(10);
    System second(7, 1000);
    second.runWorkload(10);

    EXPECT_EQ(first.sim.getTrace(), second.sim.getTrace());
}